import http.client
import threading
import json
import re
import time

# Constants
HOST = '127.0.0.1'
PORT = 2000
TIMEOUT = 60  # minutes
FAILED = False
lock = threading.Lock()

# Function to process URLs
def process_url(url):
    global FAILED
    try:
        conn = http.client.HTTPConnection(HOST, PORT, timeout=10)
        conn.request('GET', url)
        response = conn.getresponse()
        if response.status == 200:
            data = response.read().decode()
            urls = re.findall(r'src : \'([^\']+)\'', data)
            for u in urls:
                if '{ type : \'folder\'' not in data:
                    continue
                threading.Thread(target=process_url, args=(u,)).start()
        else:
            print(f'Failed to retrieve {url}. Status: {response.status}')
            with lock:
                FAILED = True
    except Exception as e:
        print(f'Error processing {url}: {e}')
        with lock:
            FAILED = True
    finally:
        conn.close()

# Main function
def main():
    global FAILED
    start_time = time.time()
    while time.time() - start_time < TIMEOUT * 60:
        try:
            conn = http.client.HTTPConnection(HOST, PORT, timeout=10)
            conn.request('GET', '/')
            response = conn.getresponse()
            if response.status == 200:
                data = response.read().decode()
                urls = re.findall(r'src : \'([^\']+)\'', data)
                threads = []
                for url in urls:
                    if '{ type : \'folder\'' in data:
                        continue
                    t = threading.Thread(target=process_url, args=(url,))
                    threads.append(t)
                    t.start()
                for t in threads:
                    t.join()
            else:
                print(f'Failed to retrieve root. Status: {response.status}')
                with lock:
                    FAILED = True
                break
        except Exception as e:
            print(f'Error: {e}')
            with lock:
                FAILED = True
            break
        finally:
            conn.close()
        time.sleep(1)
    if not FAILED:
        print('Successfully completed')

if __name__ == '__main__':
    main()
