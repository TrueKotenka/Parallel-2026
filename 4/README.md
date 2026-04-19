### Результаты запуска бенчмарка
```
--- Benchmark: 20% Write / 80% Read ---

[Coarse-Grained] Threads: 1 | Operations per second: 2740667
[Coarse-Grained] Threads: 2 | Operations per second: 1364297
[Coarse-Grained] Threads: 4 | Operations per second: 868450
[Coarse-Grained] Threads: 8 | Operations per second: 643798
[Coarse-Grained] Threads: 16 | Operations per second: 628357

[Fine-Grained  ] Threads: 1 | Operations per second: 187192
[Fine-Grained  ] Threads: 2 | Operations per second: 209325
[Fine-Grained  ] Threads: 4 | Operations per second: 143478
[Fine-Grained  ] Threads: 8 | Operations per second: 155894
[Fine-Grained  ] Threads: 16 | Operations per second: 152478

[Optimistic    ] Threads: 1 | Operations per second: 936559
[Optimistic    ] Threads: 2 | Operations per second: 1423071
[Optimistic    ] Threads: 4 | Operations per second: 2388464
[Optimistic    ] Threads: 8 | Operations per second: 3632795
[Optimistic    ] Threads: 16 | Operations per second: 4896215

[Lazy          ] Threads: 1 | Operations per second: 1876329
[Lazy          ] Threads: 2 | Operations per second: 2748099
[Lazy          ] Threads: 4 | Operations per second: 6592752
[Lazy          ] Threads: 8 | Operations per second: 10905378
[Lazy          ] Threads: 16 | Operations per second: 13099592
```
### Итоговый отчет и исследование

Основываясь на архитектуре алгоритмов и результатах бенчмарков, мы можем сделать следующие выводы о применении стратегий:

**1. Грубая синхронизация (Coarse-Grained)**
* **Условия выигрыша:** Однопоточная среда или системы, где конкуренция (contention) стремится к нулю.
* **Вывод:** В многопоточной среде быстро деградирует из-за того, что мьютекс становится узким горлышком. Потоки простаивают, ожидая блокировки.

**2. Тонкая синхронизация (Fine-Grained)**
* **Условия выигрыша:** Практически никогда не выигрывает на структурах типа «связный список». Может быть полезна в деревьях или графах.
* **Вывод:** Худший результат. Огромный накладной расход на постоянные вызовы `lock/unlock` при обходе. Кроме того, точка входа (захват головы списка) всё равно блокирует потоки, сводя на нет преимущества параллелизма.

**3. Оптимистичная синхронизация (Optimistic)**
* **Условия выигрыша:** Среды с умеренным количеством записей и чтений, когда длина списка относительно невелика.
* **Вывод:** Дает отличный прирост производительности за счет прохода без блокировок. Однако валидация $O(N)$ делает операции записи крайне медленными, если список разрастается.

**4. Ленивая синхронизация (Lazy)**
* **Условия выигрыша:** Выигрывает **всегда** в приложениях с профилем нагрузки *Read-Heavy* (как в нашем тесте: 80% или 90+ % чтений).
* **Вывод:** Идеальный выбор. Операция `contains` становится *wait-free*. Валидация при записи занимает $O(1)$ время. В высоконагруженных системах именно эта стратегия способна дать максимальную пропускную способность.