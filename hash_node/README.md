Для запуска:
``` bash
docker-compose up --build
```

Пример вывода:
``` bash
Attaching to cpp-client, cpp-server
cpp-server  | [*] server started[8080]. Awaiting connections...
cpp-server  | [*] client connected: 172.20.0.3:56172
cpp-server  | [Клиент -> Сервер] READY 500000
cpp-server  | [Сервер -> Клиент] TASK 22207b4bb7166e6034c2023bfda9358d2705f6a8 AAA ZZZ
cpp-server  | [Клиент -> Сервер] FOUND ABC
cpp-server  | [Сервер -> Клиент] DONE
cpp-server  | [*] connection closed
cpp-client  | [*] Успешное подключение к server:8080
cpp-client  | [*] Получена задача. Диапазон: [AAA ; ZZZ]
cpp-client  | [+] Ключ найден: ABC
cpp-client  | [*] Получена команда DONE. Завершение работы...
cpp-client  | [*] Программа успешно завершена.
cpp-client exited with code 0
```