# Статистика динамической трассы LLVM IR

Задание №2: инструментирующий Pass для логического модуля `app.c` динамического фрактала Жюлиа из задания №1.

## Методика

Для каждого режима отдельно: Clang оптимизирует `app.c` на `-O2` или `-Os`; `opt -passes=trace-uses` вставляет логгер в уже оптимизированный IR. Дальнейший кодоген выполняется с `-O0`, чтобы не запускать ещё один оптимизационный pipeline поверх инструментированного IR. Отключение векторизации (`-fno-vectorize -fno-slp-vectorize`) сохранено из задания №1.

В репозитории сохранены **полные рантайм-трассы** (-O2: sdl, -Os: sdl; кадров: -O2 = 2, -Os = 2) (не статический обход IR, без семплирования и обрезки). Логика, разрешение и начало отсчёта кадров одинаковы для двух режимов. `sim.c`, тестовый драйвер и логгер не инструментируются.

- `instructions.bin.gz`: один байт opcode для каждой выполненной инструкции с User != phi, включая инструкции без операндов-инструкций.
- `uses.bin.gz`: два байта (User opcode, Operand opcode) на каждую связь. Константы, параметры функций, метки и глобальные значения не являются инструкциями и не порождают пару.
- `opcodes.json`: словарь кодов именно использованной версии LLVM.
- `*_patterns_1.tsv` … `*_patterns_5.tsv`: все паттерны, частоты и доли для обоих потоков; `*_summary.json`: число событий, окон и уникальных паттернов.
- `run.json`: компилятор, параметры и SHA-256 исходников, IR и трасс; `frames.txt`: число пикселей и FNV-1a контрольная сумма каждого кадра.

- `app.optimized.ll` и `app.instrumented.ll`: точный IR этого запуска до и после Pass; `*_sample.txt`: первые 200 записей в текстовом виде.

Окна скользящие, перекрывающиеся: при N событиях для длины k имеется max(N-k+1, 0) окон. Доля — count / число окон этой длины × 100%. Поток непрерывный: окна могут пересекать границы basic block, вызовов и кадров. В потоке uses длина окна измеряется **записями пар**, а в instructions — **выполненными IR-инструкциями**. Последовательность пар не является цепочкой SSA-зависимостей.

**Задание со звёздочкой:** User phi полностью пропускается. Если операнд — phi, рекурсивно обходятся его incoming values и вместо phi печатаются opcode входящих инструкций. Например, `shl <- phi(add, sub)` даёт `shl <- add`, затем `shl <- sub` при каждом исполнении shl. Это все статические альтернативы phi, а не только выбранная на данном переходе. Порядок — порядок операндов User и incoming values phi; повторные вхождения сохраняются. Цикл из phi обрывается только на текущем пути рекурсии, остальные ветви обходятся. Обычные инструкции (например, add в loop-carried зависимости) дальше не раскрываются.

## Сравнение режимов

| Режим | Исполненные инструкции без phi | Записи User <- Operand | Кадры |
|---|---:|---:|---:|
| -O2 | 54,317,375 | 96,020,274 | 2 |
| -Os | 54,979,035 | 72,844,428 | 2 |

## Анализ часто повторяемых паттернов

В обоих режимах основную нагрузку дают `icmp`, `add`, `mul` и `br`. Это соответствует циклу `calculateIterations`: проверка границ комплексных координат, вычисление квадратов, проверка радиуса и обновление z. Сравнения и сложения занимают примерно по 21% каждого потока instructions.

Паттерн `mul ; mul ; add ; icmp ; br` встречается 2,173,242 раз на -O2 и 2,173,242 на -Os: это вычисление `z_re*z_re + z_im*z_im` и выход по радиусу. Последовательность `sub ; sdiv ; add ; shl ; mul` соответствует вычислению следующих действительной и мнимой координат в fixed-point арифметике. `shl ; xor ; select ; mul ; udiv` характерен для треугольной цветовой волны и масштабирования яркости.

Самая частая пара в этом запуске — `mul <- add`: 22,395,790 раз на -O2 и 11,327,584 на -Os. Источник — умножения значений, приходящих через phi из сложений (координаты и счётчики); раскрытие phi сохраняет все входящие альтернативы и каждое вхождение операнда. Для `mul x, x` одинаковая зависимость записывается дважды. Поэтому серия из пяти `mul <- add` в uses не означает, что исполнены пять инструкций mul подряд.

На -Os исполнено на 1.218% больше инструкций без phi, но записей пар на 24.136% меньше. В сохранённом IR на -O2 `calculateIterations` и `makeColor` встроены в `app`; на -Os они остаются отдельными вызовами. На -Os входные параметры функций не являются инструкциями и не дают пары, а на -O2 после inlining та же логика использует инструкции caller и более богатые phi. Кроме того, -O2 специализирует часть делений как udiv. Это объясняет разницу числа связей и паттернов; числа связей нельзя трактовать как число реально выполненных операций.

Счётчик `call` равен 589,826 на -O2: 589,824 вызовов simPutPixel и 2 simFlush. На -Os он равен 884,738: дополнительно выполнено по 147,456 вызовов calculateIterations и makeColor. Появление 294,912 `ret` на -Os согласуется с этими двумя функциями. Вставленные вызовы логгера в эти счётчики не входят.

## -O2

Проверка кадров:

```text
frame=0 pixels=294912 fnv1a64=44e864e95e2f7ccd
frame=1 pixels=294912 fnv1a64=3b20f8e09c974d59
```

| Поток | Длина окна | Всего окон | Уникальных паттернов |
|---|---:|---:|---:|
| instructions | 1 | 54,317,375 | 14 |
| instructions | 2 | 54,317,374 | 47 |
| instructions | 3 | 54,317,373 | 78 |
| instructions | 4 | 54,317,372 | 96 |
| instructions | 5 | 54,317,371 | 106 |
| uses | 1 | 96,020,274 | 37 |
| uses | 2 | 96,020,273 | 84 |
| uses | 3 | 96,020,272 | 110 |
| uses | 4 | 96,020,271 | 124 |
| uses | 5 | 96,020,270 | 136 |

### uses: окно 1, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `mul <- add` | 22,395,790 | 23.324% |
| `icmp <- add` | 15,903,287 | 16.562% |
| `add <- add` | 11,354,802 | 11.825% |
| `select <- icmp` | 7,125,723 | 7.421% |
| `br <- icmp` | 4,584,782 | 4.775% |
| `add <- mul` | 4,458,022 | 4.643% |
| `shl <- add` | 4,452,164 | 4.637% |
| `add <- sdiv` | 4,415,866 | 4.599% |
| `sub <- mul` | 4,304,324 | 4.483% |
| `br <- select` | 2,263,700 | 2.358% |

### uses: окно 2, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `mul <- add ; mul <- add` | 17,364,856 | 18.085% |
| `icmp <- add ; icmp <- add` | 6,791,100 | 7.073% |
| `icmp <- add ; br <- icmp` | 4,584,782 | 4.775% |
| `add <- add ; icmp <- add` | 4,563,702 | 4.753% |
| `add <- add ; add <- sdiv` | 4,304,324 | 4.483% |
| `mul <- add ; add <- mul` | 2,284,780 | 2.379% |
| `icmp <- add ; select <- icmp` | 2,263,701 | 2.358% |
| `add <- add ; add <- add` | 2,263,700 | 2.358% |
| `icmp <- add ; select <- select` | 2,263,700 | 2.358% |
| `select <- icmp ; br <- select` | 2,263,700 | 2.358% |

### uses: окно 3, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `mul <- add ; mul <- add ; mul <- add` | 13,039,452 | 13.580% |
| `add <- add ; icmp <- add ; br <- icmp` | 2,300,002 | 2.395% |
| `add <- add ; add <- add ; icmp <- add` | 2,263,700 | 2.358% |
| `add <- add ; icmp <- add ; icmp <- add` | 2,263,700 | 2.358% |
| `icmp <- add ; icmp <- add ; icmp <- add` | 2,263,700 | 2.358% |
| `icmp <- add ; icmp <- add ; select <- icmp` | 2,263,700 | 2.358% |
| `icmp <- add ; icmp <- add ; select <- select` | 2,263,700 | 2.358% |
| `icmp <- add ; select <- icmp ; select <- icmp` | 2,263,700 | 2.358% |
| `icmp <- add ; select <- select ; select <- icmp` | 2,263,700 | 2.358% |
| `select <- icmp ; icmp <- add ; icmp <- add` | 2,263,700 | 2.358% |

### uses: окно 4, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `mul <- add ; mul <- add ; mul <- add ; mul <- add` | 10,866,210 | 11.317% |
| `add <- add ; add <- add ; icmp <- add ; icmp <- add` | 2,263,700 | 2.358% |
| `add <- add ; icmp <- add ; icmp <- add ; icmp <- add` | 2,263,700 | 2.358% |
| `icmp <- add ; icmp <- add ; icmp <- add ; select <- icmp` | 2,263,700 | 2.358% |
| `icmp <- add ; icmp <- add ; select <- icmp ; select <- icmp` | 2,263,700 | 2.358% |
| `icmp <- add ; icmp <- add ; select <- select ; select <- icmp` | 2,263,700 | 2.358% |
| `icmp <- add ; select <- icmp ; select <- icmp ; icmp <- add` | 2,263,700 | 2.358% |
| `icmp <- add ; select <- select ; select <- icmp ; br <- select` | 2,263,700 | 2.358% |
| `select <- icmp ; icmp <- add ; icmp <- add ; select <- select` | 2,263,700 | 2.358% |
| `select <- icmp ; select <- icmp ; icmp <- add ; icmp <- add` | 2,263,700 | 2.358% |

### uses: окно 5, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `mul <- add ; mul <- add ; mul <- add ; mul <- add ; mul <- add` | 8,692,968 | 9.053% |
| `add <- add ; add <- add ; icmp <- add ; icmp <- add ; icmp <- add` | 2,263,700 | 2.358% |
| `add <- add ; icmp <- add ; icmp <- add ; icmp <- add ; select <- icmp` | 2,263,700 | 2.358% |
| `icmp <- add ; icmp <- add ; icmp <- add ; select <- icmp ; select <- icmp` | 2,263,700 | 2.358% |
| `icmp <- add ; icmp <- add ; select <- icmp ; select <- icmp ; icmp <- add` | 2,263,700 | 2.358% |
| `icmp <- add ; icmp <- add ; select <- select ; select <- icmp ; br <- select` | 2,263,700 | 2.358% |
| `icmp <- add ; select <- icmp ; select <- icmp ; icmp <- add ; icmp <- add` | 2,263,700 | 2.358% |
| `select <- icmp ; icmp <- add ; icmp <- add ; select <- select ; select <- icmp` | 2,263,700 | 2.358% |
| `select <- icmp ; select <- icmp ; icmp <- add ; icmp <- add ; select <- select` | 2,263,700 | 2.358% |
| `br <- select ; mul <- add ; mul <- add ; mul <- add ; mul <- add` | 2,173,242 | 2.263% |

### instructions: окно 1, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `icmp` | 11,710,505 | 21.559% |
| `add` | 11,635,271 | 21.421% |
| `mul` | 7,204,180 | 13.263% |
| `br` | 7,107,864 | 13.086% |
| `select` | 4,862,023 | 8.951% |
| `sdiv` | 4,304,330 | 7.924% |
| `shl` | 2,857,692 | 5.261% |
| `sub` | 2,152,166 | 3.962% |
| `or` | 630,294 | 1.160% |
| `udiv` | 593,992 | 1.094% |

### instructions: окно 2, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `add ; icmp` | 6,848,487 | 12.608% |
| `icmp ; br` | 4,584,782 | 8.441% |
| `icmp ; select` | 4,527,405 | 8.335% |
| `sdiv ; add` | 4,304,328 | 7.924% |
| `br ; mul` | 2,432,620 | 4.479% |
| `mul ; add` | 2,284,780 | 4.206% |
| `br ; add` | 2,264,084 | 4.168% |
| `select ; br` | 2,263,701 | 4.168% |
| `icmp ; icmp` | 2,263,700 | 4.168% |
| `select ; icmp` | 2,263,700 | 4.168% |

### instructions: окно 3, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `add ; icmp ; br` | 4,473,244 | 8.235% |
| `br ; add ; icmp` | 2,264,084 | 4.168% |
| `icmp ; select ; br` | 2,263,701 | 4.168% |
| `add ; icmp ; icmp` | 2,263,700 | 4.168% |
| `icmp ; icmp ; select` | 2,263,700 | 4.168% |
| `icmp ; select ; icmp` | 2,263,700 | 4.168% |
| `select ; icmp ; select` | 2,263,700 | 4.168% |
| `br ; mul ; mul` | 2,173,242 | 4.001% |
| `mul ; add ; icmp` | 2,173,242 | 4.001% |
| `mul ; mul ; add` | 2,173,242 | 4.001% |

### instructions: окно 4, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `br ; add ; icmp ; icmp` | 2,263,700 | 4.168% |
| `add ; icmp ; icmp ; select` | 2,263,700 | 4.168% |
| `icmp ; icmp ; select ; icmp` | 2,263,700 | 4.168% |
| `icmp ; select ; icmp ; select` | 2,263,700 | 4.168% |
| `select ; icmp ; select ; br` | 2,263,700 | 4.168% |
| `br ; mul ; mul ; add` | 2,173,242 | 4.001% |
| `mul ; add ; icmp ; br` | 2,173,242 | 4.001% |
| `mul ; mul ; add ; icmp` | 2,173,242 | 4.001% |
| `icmp ; select ; br ; mul` | 2,173,242 | 4.001% |
| `select ; br ; mul ; mul` | 2,173,242 | 4.001% |

### instructions: окно 5, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `br ; add ; icmp ; icmp ; select` | 2,263,700 | 4.168% |
| `add ; icmp ; icmp ; select ; icmp` | 2,263,700 | 4.168% |
| `icmp ; icmp ; select ; icmp ; select` | 2,263,700 | 4.168% |
| `icmp ; select ; icmp ; select ; br` | 2,263,700 | 4.168% |
| `br ; mul ; mul ; add ; icmp` | 2,173,242 | 4.001% |
| `mul ; mul ; add ; icmp ; br` | 2,173,242 | 4.001% |
| `icmp ; select ; br ; mul ; mul` | 2,173,242 | 4.001% |
| `select ; br ; mul ; mul ; add` | 2,173,242 | 4.001% |
| `select ; icmp ; select ; br ; mul` | 2,173,242 | 4.001% |
| `br ; sub ; sdiv ; add ; shl` | 2,152,162 | 3.962% |

## -Os

Проверка кадров:

```text
frame=0 pixels=294912 fnv1a64=44e864e95e2f7ccd
frame=1 pixels=294912 fnv1a64=3b20f8e09c974d59
```

| Поток | Длина окна | Всего окон | Уникальных паттернов |
|---|---:|---:|---:|
| instructions | 1 | 54,979,035 | 15 |
| instructions | 2 | 54,979,034 | 50 |
| instructions | 3 | 54,979,033 | 80 |
| instructions | 4 | 54,979,032 | 99 |
| instructions | 5 | 54,979,031 | 111 |
| uses | 1 | 72,844,428 | 42 |
| uses | 2 | 72,844,427 | 90 |
| uses | 3 | 72,844,426 | 110 |
| uses | 4 | 72,844,425 | 124 |
| uses | 5 | 72,844,424 | 136 |

### uses: окно 1, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `mul <- add` | 11,327,584 | 15.550% |
| `icmp <- add` | 11,264,349 | 15.464% |
| `select <- icmp` | 7,125,723 | 9.782% |
| `add <- add` | 4,786,778 | 6.571% |
| `br <- icmp` | 4,620,700 | 6.343% |
| `add <- mul` | 4,458,022 | 6.120% |
| `add <- sdiv` | 4,415,866 | 6.062% |
| `sub <- mul` | 4,304,324 | 5.909% |
| `sdiv <- mul` | 2,598,318 | 3.567% |
| `shl <- add` | 2,300,002 | 3.157% |

### uses: окно 2, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `mul <- add ; mul <- add` | 6,519,726 | 8.950% |
| `add <- add ; icmp <- add` | 4,563,702 | 6.265% |
| `icmp <- add ; br <- icmp` | 4,473,244 | 6.141% |
| `mul <- add ; sdiv <- mul` | 2,486,776 | 3.414% |
| `sdiv <- mul ; add <- sdiv` | 2,263,704 | 3.108% |
| `icmp <- add ; select <- icmp` | 2,263,701 | 3.108% |
| `icmp <- add ; icmp <- add` | 2,263,700 | 3.108% |
| `icmp <- add ; select <- select` | 2,263,700 | 3.108% |
| `select <- icmp ; br <- select` | 2,263,700 | 3.108% |
| `select <- icmp ; icmp <- add` | 2,263,700 | 3.108% |

### uses: окно 3, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `mul <- add ; mul <- add ; mul <- add` | 4,346,484 | 5.967% |
| `add <- add ; icmp <- add ; br <- icmp` | 2,300,002 | 3.157% |
| `add <- add ; icmp <- add ; icmp <- add` | 2,263,700 | 3.108% |
| `icmp <- add ; icmp <- add ; select <- icmp` | 2,263,700 | 3.108% |
| `icmp <- add ; select <- icmp ; select <- icmp` | 2,263,700 | 3.108% |
| `icmp <- add ; select <- select ; select <- icmp` | 2,263,700 | 3.108% |
| `select <- icmp ; icmp <- add ; select <- select` | 2,263,700 | 3.108% |
| `select <- icmp ; select <- icmp ; icmp <- add` | 2,263,700 | 3.108% |
| `select <- select ; select <- icmp ; br <- select` | 2,263,700 | 3.108% |
| `br <- select ; mul <- add ; mul <- add` | 2,173,242 | 2.983% |

### uses: окно 4, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `add <- add ; icmp <- add ; icmp <- add ; select <- icmp` | 2,263,700 | 3.108% |
| `icmp <- add ; icmp <- add ; select <- icmp ; select <- icmp` | 2,263,700 | 3.108% |
| `icmp <- add ; select <- icmp ; select <- icmp ; icmp <- add` | 2,263,700 | 3.108% |
| `icmp <- add ; select <- select ; select <- icmp ; br <- select` | 2,263,700 | 3.108% |
| `select <- icmp ; icmp <- add ; select <- select ; select <- icmp` | 2,263,700 | 3.108% |
| `select <- icmp ; select <- icmp ; icmp <- add ; select <- select` | 2,263,700 | 3.108% |
| `br <- select ; mul <- add ; mul <- add ; mul <- add` | 2,173,242 | 2.983% |
| `add <- mul ; add <- mul ; icmp <- add ; br <- icmp` | 2,173,242 | 2.983% |
| `mul <- add ; add <- mul ; add <- mul ; icmp <- add` | 2,173,242 | 2.983% |
| `mul <- add ; mul <- add ; add <- mul ; add <- mul` | 2,173,242 | 2.983% |

### uses: окно 5, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `add <- add ; icmp <- add ; icmp <- add ; select <- icmp ; select <- icmp` | 2,263,700 | 3.108% |
| `icmp <- add ; icmp <- add ; select <- icmp ; select <- icmp ; icmp <- add` | 2,263,700 | 3.108% |
| `icmp <- add ; select <- icmp ; select <- icmp ; icmp <- add ; select <- select` | 2,263,700 | 3.108% |
| `select <- icmp ; icmp <- add ; select <- select ; select <- icmp ; br <- select` | 2,263,700 | 3.108% |
| `select <- icmp ; select <- icmp ; icmp <- add ; select <- select ; select <- icmp` | 2,263,700 | 3.108% |
| `br <- select ; mul <- add ; mul <- add ; mul <- add ; mul <- add` | 2,173,242 | 2.983% |
| `mul <- add ; add <- mul ; add <- mul ; icmp <- add ; br <- icmp` | 2,173,242 | 2.983% |
| `mul <- add ; mul <- add ; add <- mul ; add <- mul ; icmp <- add` | 2,173,242 | 2.983% |
| `mul <- add ; mul <- add ; mul <- add ; add <- mul ; add <- mul` | 2,173,242 | 2.983% |
| `mul <- add ; mul <- add ; mul <- add ; mul <- add ; add <- mul` | 2,173,242 | 2.983% |

### instructions: окно 1, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `icmp` | 11,746,423 | 21.365% |
| `add` | 11,635,271 | 21.163% |
| `mul` | 7,204,180 | 13.104% |
| `br` | 7,143,782 | 12.994% |
| `select` | 4,862,023 | 8.843% |
| `sdiv` | 4,750,482 | 8.641% |
| `shl` | 2,857,692 | 5.198% |
| `sub` | 2,152,166 | 3.915% |
| `call` | 884,738 | 1.609% |
| `or` | 630,294 | 1.146% |

### instructions: окно 2, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `add ; icmp` | 6,848,487 | 12.457% |
| `sdiv ; add` | 4,638,942 | 8.438% |
| `icmp ; br` | 4,620,700 | 8.404% |
| `icmp ; select` | 4,527,405 | 8.235% |
| `mul ; sdiv` | 2,598,318 | 4.726% |
| `br ; mul` | 2,432,620 | 4.425% |
| `mul ; add` | 2,284,780 | 4.156% |
| `br ; add` | 2,264,084 | 4.118% |
| `select ; br` | 2,263,701 | 4.117% |
| `icmp ; icmp` | 2,263,700 | 4.117% |

### instructions: окно 3, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `add ; icmp ; br` | 4,473,244 | 8.136% |
| `mul ; sdiv ; add` | 2,486,780 | 4.523% |
| `br ; add ; icmp` | 2,264,084 | 4.118% |
| `icmp ; select ; br` | 2,263,701 | 4.117% |
| `add ; icmp ; icmp` | 2,263,700 | 4.117% |
| `icmp ; icmp ; select` | 2,263,700 | 4.117% |
| `icmp ; select ; icmp` | 2,263,700 | 4.117% |
| `select ; icmp ; select` | 2,263,700 | 4.117% |
| `br ; mul ; mul` | 2,173,242 | 3.953% |
| `mul ; add ; icmp` | 2,173,242 | 3.953% |

### instructions: окно 4, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `br ; add ; icmp ; icmp` | 2,263,700 | 4.117% |
| `add ; icmp ; icmp ; select` | 2,263,700 | 4.117% |
| `icmp ; icmp ; select ; icmp` | 2,263,700 | 4.117% |
| `icmp ; select ; icmp ; select` | 2,263,700 | 4.117% |
| `select ; icmp ; select ; br` | 2,263,700 | 4.117% |
| `br ; mul ; mul ; add` | 2,173,242 | 3.953% |
| `mul ; add ; icmp ; br` | 2,173,242 | 3.953% |
| `mul ; mul ; add ; icmp` | 2,173,242 | 3.953% |
| `icmp ; select ; br ; mul` | 2,173,242 | 3.953% |
| `select ; br ; mul ; mul` | 2,173,242 | 3.953% |

### instructions: окно 5, топ-10

| Паттерн | Количество | Доля окон |
|---|---:|---:|
| `br ; add ; icmp ; icmp ; select` | 2,263,700 | 4.117% |
| `add ; icmp ; icmp ; select ; icmp` | 2,263,700 | 4.117% |
| `icmp ; icmp ; select ; icmp ; select` | 2,263,700 | 4.117% |
| `icmp ; select ; icmp ; select ; br` | 2,263,700 | 4.117% |
| `br ; mul ; mul ; add ; icmp` | 2,173,242 | 3.953% |
| `mul ; mul ; add ; icmp ; br` | 2,173,242 | 3.953% |
| `icmp ; select ; br ; mul ; mul` | 2,173,242 | 3.953% |
| `select ; br ; mul ; mul ; add` | 2,173,242 | 3.953% |
| `select ; icmp ; select ; br ; mul` | 2,173,242 | 3.953% |
| `br ; sub ; sdiv ; add ; shl` | 2,152,162 | 3.915% |

## Воспроизведение

```bash
make trace-test
make trace TRACE_FRAMES=2
make trace-analysis
python3 tools/decode_trace.py statistics/O2 --limit 40
```

Для машины без графического дисплея: `make trace-headless`. Эта команда перезаписывает статистику трассами headless-режима. Тест проверяет, что на двух кадрах они побайтно совпадают с SDL-трассами.

Исходные лекционные примеры: [Pass4_uses.cpp](https://github.com/lisitsynSA/llvm_course/blob/main/LLVM_Pass/Pass4_uses.cpp) и [Pass6_cfg.cpp](https://github.com/lisitsynSA/llvm_course/blob/main/LLVM_Pass/Pass6_cfg.cpp).
