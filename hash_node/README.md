Для запуска:
``` bash
docker-compose up --build
```

Пример вывода:
``` bash
Attaching to cpp-client, mock-server
mock-server  | [*] Сервер запущен. Ожидание подключений на порту 8080...
mock-server  | [*] Подключен клиент: ('172.19.0.3', 53520)
mock-server  | [Клиент -> Сервер] READY 500000
mock-server  | [Сервер -> Клиент] TASK 22207b4bb7166e6034c2023bfda9358d2705f6a8 AAA ZZZ
mock-server  | [Клиент -> Сервер] FOUND ABC
mock-server  | [Сервер -> Клиент] DONE
mock-server  | [*] Соединение закрыто. Сервер завершает работу.
cpp-client   | [*] Успешное подключение к server:8080
cpp-client   | [*] Получена задача. Диапазон: [AAA ; ZZZ]
cpp-client   | [+] Ключ найден: ABC
cpp-client   | [*] Получена команда DONE. Завершение работы...
cpp-client   | [*] Программа успешно завершена.
cpp-client exited with code 0
mock-server exited with code 0
```