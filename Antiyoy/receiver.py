import sys
import socket
import struct
import select

from enum import IntEnum

# Nazewnictwo i kolejność odpowiadają tym z gry, ich zmiana może uszkodzić rozczytywanie planszy
class Resident(IntEnum):
    Water = 0 # Woda liczy się jako rezydent
    Empty = 1

    Warrior1 = 2
    Warrior2 = 3
    Warrior3 = 4
    Warrior4 = 5

    Farm = 6
    Castle = 7
    Tower = 8
    StrongTower = 9

    PalmTree = 10
    PineTree = 11
    Gravestone = 12

class Hex:
    def __init__(self, x, y, owner_id, resident):
        self.x = x
        self.y = y
        self.owner_id = owner_id
        self.resident = resident

    def __repr__(self):
        return f"Hex(x={self.x}, y={self.y}, owner={self.owner_id}, resident={self.resident})"

class Board:
    def __init__(self, width, height):
        self.width = width
        self.height = height
        self.hexes = []

    def add_hex(self, hexagon):
        self.hexes.append(hexagon)

    def __repr__(self):
        return f"Board({self.width}x{self.height}, {len(self.hexes)} hexes)"



MAGIC_SOCKET_TAG = 0 # Magiczne numerki wysyłane na początku do socketa by mieć pewność że jesteśmy odpowiednio połączeni
CONFIGURATION_SOCKET_TAG = 1 # Dane gry wysyłane przy rozpoczęciu nowej gry
CONFIRMATION_SOCKET_TAG = 2 # Potwierdzenie wysyłane przez grę po otrzymaniu ruchu składające się z 2 booleanów: czy zatwierdzono ruch oraz czy nadal wyczekuje ruchu
BOARD_SOCKET_TAG = 3 # Plansza (spłaszczona dwuwymiarowa tablica heksagonów)
MOVE_SOCKET_TAG = 4 # Ruchy (wysyłane przez AI do gry)

SOCKET_MAGIC_NUMBERS = b'ANTIYOY'

sock = None # Socket


def receive_magic() -> bool:
    """
    Odbiera magiczne numerki, zwraca bool czy pasują czy nie. Zakłada że tag został już odebrany
    """
    magic_len = len(SOCKET_MAGIC_NUMBERS)
    data = bytearray()
    while len(data) < magic_len:
        chunk = sock.recv(magic_len - len(data))
        if not chunk:
            raise RuntimeError("Nie udało się pobrać magicznych numerków")
        data.extend(chunk)

    return data == SOCKET_MAGIC_NUMBERS

def receive_confirmation() -> tuple[bool, bool]:
    """
    Odbiera potwierdzenie ruchu (czy zatwierdzono i czy oczekuje dalszych ruchów). Zakłada że tag został już odebrany
    """
    data = bytearray()
    while len(data) < 2:
        chunk = sock.recv(2 - len(data))
        if not chunk:
            raise RuntimeError("Nie udało się pobrać potwierdzenia")
        data.extend(chunk)

    approved = bool(data[0])
    awaiting = bool(data[1])
    return approved, awaiting

def receive_board() -> Board:
    """
    Odbiera planszę. Zakłada że tag został już odebrany
    """
    header_size = 4
    header = sock.recv(header_size)
    if len(header) < header_size:
        raise RuntimeError("Nie udało się pobrać nagłówka")

    width, height = struct.unpack('!HH', header)

    hexes_number = width * height
    data_size = hexes_number * 2 # Właściciel i rezydent (oba po bajcie)

    data = bytearray()
    while len(data) < data_size:
        chunk = sock.recv(data_size - len(data))
        if not chunk:
            raise RuntimeError("Nie udało się pobrać wszystkich danych")
        data.extend(chunk)

    board = Board(width, height)

    for y in range(height):
        for x in range(width):
            idx = (y * width + x) * 2
            owner_id = data[idx]
            resident_value = data[idx + 1]
            board.add_hex(Hex(x, y, owner_id, Resident(resident_value)))

    return board


def receive_all():
    """
    Odbiera wszystko z socketa
    """
    result = []
    tag = sock.recv(1)
    while len(tag) > 0:
        tag = struct.unpack('B', tag)[0]

        if tag == MAGIC_SOCKET_TAG:
            result.append((MAGIC_SOCKET_TAG, receive_magic()))

        elif tag == CONFIRMATION_SOCKET_TAG:
            result.append((CONFIRMATION_SOCKET_TAG, receive_confirmation()))

        elif tag == BOARD_SOCKET_TAG:
            result.append((BOARD_SOCKET_TAG, receive_board()))
        
        else: # Odebrano nieznany tag, socket najpewniej jest zawalony śmieciami z którymi nie wiemy co zrobić, nie możemy wydedukować stanu gry, trzeba zakończyć
            sock.close()
            raise RuntimeError("Odebrano błędne dane")

        ready, _, _ = select.select([sock], [], [], 0)
        if not ready: # Jeśli nie ma więcej danych do pobrania kończymy
            return result

        tag = sock.recv(1)

    return result


# Program odpalany przez std::system("python3 receiver.py 127.0.0.1 2137");
HOST = '127.0.0.1'
PORT = 2137

if len(sys.argv) >= 2:
    HOST = sys.argv[1]
if len(sys.argv) >= 3:
    PORT = int(sys.argv[2])

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))

while True:
    received = receive_all() # Czeka na dane

    if not received:
        continue # Nie powinno się zdarzyć, ale dla bezpieczeństwa

    for tag, payload in received:
        if tag == MAGIC_SOCKET_TAG:
            if payload:
                print("Działa!")
            else:
                print("Magic numbers złe!")

        elif tag == CONFIRMATION_SOCKET_TAG:
            approved, awaiting = payload
            print("Potwierdzenie:", approved, awaiting)

        elif tag == BOARD_SOCKET_TAG:
            board = payload
            print("Nowa plansza:")
            print(board)
