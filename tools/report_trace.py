"""Generate a compact report from complete runtime frequency tables."""
import csv
import json
from pathlib import Path
import sys


def table(path, limit=10):
    with path.open() as file:
        rows = list(csv.DictReader(file, delimiter="\t"))[:limit]
    lines = ["| Паттерн | Количество | Доля окон |", "|---|---:|---:|"]
    lines += [f"| `{row['pattern']}` | {int(row['count']):,} | {float(row['percent']):.3f}% |"
              for row in rows]
    return lines


def main():
    root = Path(sys.argv[1])
    runs = {mode: json.loads((root / mode / "run.json").read_text()) for mode in ("O2", "Os")}
    frame_counts = {mode: runs[mode]["frames"] for mode in runs}
    backends = ", ".join(f"-{mode}: {runs[mode]['backend']}" for mode in runs)
    lines = ["# Статистика динамической трассы LLVM IR", "",
             "Задание №2: инструментирующий Pass для логического модуля `app.c` "
             "динамического фрактала Жюлиа из задания №1.", "",
             "## Методика", "",
             "Для каждого режима отдельно: Clang оптимизирует `app.c` на `-O2` или `-Os`; "
             "`opt -passes=trace-uses` вставляет логгер в уже оптимизированный IR. "
             "Дальнейший кодоген выполняется с `-O0`, чтобы не запускать ещё один "
             "оптимизационный pipeline поверх инструментированного IR. Отключение "
             "векторизации (`-fno-vectorize -fno-slp-vectorize`) сохранено из задания №1.", "",
             f"В репозитории сохранены **полные рантайм-трассы** ({backends}; "
             f"кадров: -O2 = {frame_counts['O2']}, -Os = {frame_counts['Os']}) "
             "(не статический обход IR, без семплирования и обрезки). "
             "Логика, разрешение и начало отсчёта кадров одинаковы для двух режимов. "
             "`sim.c`, тестовый драйвер и логгер не инструментируются.", "",
             "- `instructions.bin.gz`: один байт opcode для каждой выполненной инструкции "
             "с User != phi, включая инструкции без операндов-инструкций.",
             "- `uses.bin.gz`: два байта (User opcode, Operand opcode) на каждую связь. "
             "Константы, параметры функций, метки и глобальные значения не являются "
             "инструкциями и не порождают пару.",
             "- `opcodes.json`: словарь кодов именно использованной версии LLVM.",
             "- `*_patterns_1.tsv` … `*_patterns_5.tsv`: все паттерны, частоты и доли "
             "для обоих потоков; `*_summary.json`: число событий, окон и уникальных паттернов.",
             "- `run.json`: компилятор, параметры и SHA-256 исходников, IR и трасс; "
             "`frames.txt`: число пикселей и FNV-1a контрольная сумма каждого кадра.", "",
             "- `app.optimized.ll` и `app.instrumented.ll`: точный IR этого запуска "
             "до и после Pass; `*_sample.txt`: первые 200 записей в текстовом виде.", "",
             "Окна скользящие, перекрывающиеся: при N событиях для длины k имеется "
             "max(N-k+1, 0) окон. Доля — count / число окон этой длины × 100%. "
             "Поток непрерывный: окна могут пересекать границы basic block, вызовов "
             "и кадров. В потоке uses длина окна измеряется **записями пар**, "
             "а в instructions — **выполненными IR-инструкциями**. "
             "Последовательность пар не является цепочкой SSA-зависимостей.", "",
             "**Задание со звёздочкой:** User phi полностью пропускается. Если операнд — "
             "phi, рекурсивно обходятся его incoming values и вместо phi печатаются "
             "opcode входящих инструкций. Например, `shl <- phi(add, sub)` даёт "
             "`shl <- add`, затем `shl <- sub` при каждом исполнении shl. "
             "Это все статические альтернативы phi, а не только выбранная на данном "
             "переходе. Порядок — порядок операндов User и incoming values phi; "
             "повторные вхождения сохраняются. Цикл из phi обрывается только на "
             "текущем пути рекурсии, остальные ветви обходятся. Обычные инструкции "
             "(например, add в loop-carried зависимости) дальше не раскрываются.", "",
             "## Сравнение режимов", "",
             "| Режим | Исполненные инструкции без phi | Записи User <- Operand | Кадры |",
             "|---|---:|---:|---:|"]
    combined = {}
    for mode in ("O2", "Os"):
        data = {kind: json.loads((root / mode / f"{kind}_summary.json").read_text())
                for kind in ("instructions", "uses")}
        combined[mode] = data
        lines.append(f"| -{mode} | {data['instructions']['events']:,} | "
                     f"{data['uses']['events']:,} | {frame_counts[mode]} |")
    def frequency(mode, kind, length, pattern):
        with (root / mode / f"{kind}_patterns_{length}.tsv").open() as file:
            return next((int(row['count']) for row in csv.DictReader(file, delimiter='\t')
                         if row['pattern'] == pattern), 0)
    instruction_change = 100 * (combined["Os"]["instructions"]["events"] /
                                combined["O2"]["instructions"]["events"] - 1)
    edge_reduction = 100 * (1 - combined["Os"]["uses"]["events"] /
                           combined["O2"]["uses"]["events"])
    lines += ["", "## Анализ часто повторяемых паттернов", "",
              "В обоих режимах основную нагрузку дают `icmp`, `add`, `mul` и `br`. "
              "Это соответствует циклу `calculateIterations`: проверка границ комплексных "
              "координат, вычисление квадратов, проверка радиуса и обновление z. "
              "Сравнения и сложения занимают примерно по 21% каждого потока instructions.", "",
              f"Паттерн `mul ; mul ; add ; icmp ; br` встречается "
              f"{frequency('O2', 'instructions', 5, 'mul ; mul ; add ; icmp ; br'):,} "
              f"раз на -O2 и {frequency('Os', 'instructions', 5, 'mul ; mul ; add ; icmp ; br'):,} "
              "на -Os: это вычисление `z_re*z_re + z_im*z_im` и выход "
              "по радиусу. Последовательность `sub ; sdiv ; add ; shl ; mul` "
              "соответствует вычислению следующих действительной и мнимой координат "
              "в fixed-point арифметике. `shl ; xor ; select ; mul ; udiv` "
              "характерен для треугольной цветовой волны и масштабирования яркости.", "",
              f"Самая частая пара в этом запуске — `mul <- add`: "
              f"{frequency('O2', 'uses', 1, 'mul <- add'):,} раз на -O2 "
              f"и {frequency('Os', 'uses', 1, 'mul <- add'):,} на -Os. "
              "Источник — умножения значений, приходящих "
              "через phi из сложений (координаты и счётчики); раскрытие phi "
              "сохраняет все входящие альтернативы и каждое вхождение операнда. "
              "Для `mul x, x` одинаковая зависимость записывается дважды. "
              "Поэтому серия из пяти `mul <- add` в uses не означает, что "
              "исполнены пять инструкций mul подряд.", "",
              f"На -Os исполнено на {instruction_change:.3f}% больше инструкций без phi, "
              f"но записей пар на {edge_reduction:.3f}% меньше. В сохранённом IR на -O2 "
              "`calculateIterations` и `makeColor` встроены в `app`; на -Os "
              "они остаются отдельными вызовами. На -Os входные параметры функций "
              "не являются инструкциями и не дают пары, а на -O2 после inlining "
              "та же логика использует инструкции caller и более богатые phi. "
              "Кроме того, -O2 специализирует часть делений как udiv. "
              "Это объясняет разницу числа связей и паттернов; числа связей "
              "нельзя трактовать как число реально выполненных операций.", "",
              f"Счётчик `call` равен {frequency('O2', 'instructions', 1, 'call'):,} "
              f"на -O2: {frame_counts['O2'] * 294912:,} вызовов simPutPixel "
              f"и {frame_counts['O2']} simFlush. На -Os он равен "
              f"{frequency('Os', 'instructions', 1, 'call'):,}: дополнительно выполнено "
              f"по {frame_counts['Os'] * 73728:,} вызовов calculateIterations и makeColor. "
              f"Появление {frequency('Os', 'instructions', 1, 'ret'):,} `ret` "
              "на -Os согласуется с этими двумя функциями. "
              "Вставленные вызовы логгера в эти счётчики не входят."]
    for mode in ("O2", "Os"):
        lines += ["", f"## -{mode}", "", "Проверка кадров:", "", "```text",
                  (root / mode / "frames.txt").read_text().strip(), "```", "",
                  "| Поток | Длина окна | Всего окон | Уникальных паттернов |",
                  "|---|---:|---:|---:|"]
        for kind in ("instructions", "uses"):
            for row in combined[mode][kind]["windows"]:
                lines.append(f"| {kind} | {row['length']} | {row['total']:,} | {row['unique']} |")
        for kind in ("uses", "instructions"):
            for length in range(1, 6):
                lines += ["", f"### {kind}: окно {length}, топ-10", ""]
                lines += table(root / mode / f"{kind}_patterns_{length}.tsv")
    lines += ["", "## Воспроизведение", "", "```bash", "make trace-test",
              "make trace TRACE_FRAMES=2", "make trace-analysis",
              "python3 tools/decode_trace.py statistics/O2 --limit 40", "```", "",
              "Для машины без графического дисплея: `make trace-headless`. "
              "Эта команда перезаписывает статистику трассами headless-режима. "
              "Тест проверяет, что на двух кадрах они побайтно совпадают с SDL-трассами.", "",
              "Исходные лекционные примеры: "
              "[Pass4_uses.cpp](https://github.com/lisitsynSA/llvm_course/blob/main/LLVM_Pass/Pass4_uses.cpp) "
              "и [Pass6_cfg.cpp](https://github.com/lisitsynSA/llvm_course/blob/main/LLVM_Pass/Pass6_cfg.cpp).", ""]
    (root / "README.md").write_text("\n".join(lines), encoding="utf-8")
    (root / "summary.json").write_text(json.dumps(combined, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
