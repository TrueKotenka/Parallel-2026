import socket
import time
import hashlib

HOST = '0.0.0.0'
PORT = 8080

def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen()
        print(f"[*] Сервер запущен. Ожидание подключений на порту {PORT}...")
        
        conn, addr = s.accept()
        with conn:
            print(f"[*] Подключен клиент: {addr}")
            
            data = conn.recv(1024).decode('utf-8')
            print(f"[Клиент -> Сервер] {data.strip()}")
            
            if data.startswith("READY"):
                time.sleep(1) 
                
                secret_key = "ABC"
                md5_hash = hashlib.md5(secret_key.encode('utf-8')).hexdigest()
                real_target_hash = hashlib.sha1(md5_hash.encode('utf-8')).hexdigest()
                
                start_key = "AAA"
                end_key = "ZZZ"
                
                task_msg = f"TASK {real_target_hash} {start_key} {end_key}\n"
                print(f"[Сервер -> Клиент] {task_msg.strip()}")
                conn.sendall(task_msg.encode('utf-8'))
                
                result = conn.recv(1024).decode('utf-8')
                print(f"[Клиент -> Сервер] {result.strip()}")
                
                time.sleep(1)
                
                done_msg = "DONE\n"
                print(f"[Сервер -> Клиент] {done_msg.strip()}")
                conn.sendall(done_msg.encode('utf-8'))
                
            print("[*] Соединение закрыто. Сервер завершает работу.")

if __name__ == "__main__":
    start_server()