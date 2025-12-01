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

    Warrior1Moved = 6
    Warrior2Moved = 7
    Warrior3Moved = 8
    Warrior4Moved = 9

    Farm = 10
    Castle = 11
    Tower = 12
    StrongTower = 13

    PalmTree = 14
    PineTree = 15
    Gravestone = 16

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
BOARD_SOCKET_TAG = 2 # Plansza (spłaszczona dwuwymiarowa tablica heksagonów)
ACTION_SOCKET_TAG = 3 # Ruchy (wysyłane przez AI do gry)
CONFIRMATION_SOCKET_TAG = 4 # Potwierdzenie wysyłane przez grę po otrzymaniu ruchu składające się z 2 booleanów: czy zatwierdzono ruch oraz czy nadal wyczekuje ruchu
TURN_CHANGE_SOCKET_TAG = 5 # Numer gracza zaczynającego turę (zaczynając od 1, nie od 0 bo gra uznaje 0 za brak gracza)
GAME_OVER_SOCKET_TAG = 6 # Numery graczy w kolejności od wygranego do pierwszego który odpadł

SOCKET_MAGIC_NUMBERS = b'ANTIYOY'

sock = None # Socket



def recv_size(sock, size):
    """Odbiera size bajtów lub rzuca wyjątek"""
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise RuntimeError("Socket disconnected during recv_all()")
        data.extend(chunk)
    return bytes(data)


def receive_magic() -> bool:
    """
    Odbiera magiczne numerki, zwraca bool czy pasują czy nie.
    Zakłada że tag został już odebrany
    """
    magic_len = len(SOCKET_MAGIC_NUMBERS)
    data = bytearray()
    while len(data) < magic_len:
        chunk = sock.recv(magic_len - len(data))
        if not chunk:
            raise RuntimeError("Failed to receive magic numbers")
        data.extend(chunk)

    return data == SOCKET_MAGIC_NUMBERS

def receive_config():
    """
    Odbiera GameConfigData.
    Zakłada że tag został już odebrany
    """

    # 1) x, y (uint16)
    header = recv_size(sock, 2 + 2)
    x, y = struct.unpack("!HH", header)

    # 2) seed, minProvinceSize, maxProvinceSize (uint32)
    numbers = recv_size(sock, 4 * 3)
    seed, minProv, maxProv = struct.unpack("!III", numbers)

    # 3) Rozmiar playerMarkers (bajt)
    size_player_markers = recv_size(sock, 1)[0]

    # 4) Zawartość playerMarkers
    markers_raw = recv_size(sock, size_player_markers)
    playerMarkers = markers_raw.decode("ascii")

    # 5) Rozmiar maxMoveTimes (bajt)
    size_move_times = recv_size(sock, 1)[0]

    # 6) Zawartość maxMoveTimes
    move_times_raw = recv_size(sock, 4 * size_move_times)
    maxMoveTimes = list(struct.unpack("!" + "I" * size_move_times, move_times_raw))

    return {
        "x": x,
        "y": y,
        "seed": seed,
        "minProvinceSize": minProv,
        "maxProvinceSize": maxProv,
        "playerMarkers": playerMarkers,
        "maxMoveTimes": maxMoveTimes,
    }

def receive_board() -> Board:
    """
    Odbiera planszę.
    Zakłada że tag został już odebrany
    """
    header_size = 4
    header = sock.recv(header_size)
    if len(header) < header_size:
        raise RuntimeError("Failed to receive header")

    width, height = struct.unpack('!HH', header)

    hexes_number = width * height
    data_size = hexes_number * 2 # Właściciel i rezydent (oba po bajcie)

    data = bytearray()
    while len(data) < data_size:
        chunk = sock.recv(data_size - len(data))
        if not chunk:
            raise RuntimeError("Failed to receive all data")
        data.extend(chunk)

    board = Board(width, height)

    for y in range(height):
        for x in range(width):
            idx = (y * width + x) * 2
            owner_id = data[idx]
            resident_value = data[idx + 1]
            board.add_hex(Hex(x, y, owner_id, Resident(resident_value)))

    return board

def receive_confirmation() -> tuple[bool, bool]:
    """
    Odbiera potwierdzenie ruchu (czy zatwierdzono i czy oczekuje dalszych ruchów).
    Zakłada że tag został już odebrany
    """
    data = bytearray()
    while len(data) < 2:
        chunk = sock.recv(2 - len(data))
        if not chunk:
            raise RuntimeError("Failed to receive confirmation")
        data.extend(chunk)

    approved = bool(data[0])
    awaiting = bool(data[1])
    return approved, awaiting

def receive_turn_change() -> int:
    """
    Odbiera numer gracza zaczynającego turę.
    Zakłada że tag został już odebrany
    """
    data = sock.recv(1)
    if len(data) < 1:
        raise RuntimeError("Failed to receive player number (turn change)")
    return data[0]

def receive_game_over() -> list[int]:
    """
    Odbiera listę wyników (kolejność graczy od wygranego do ostatniego).
    Zakłada że tag został już odebrany
    """
    size_data = sock.recv(1)
    if len(size_data) < 1:
        raise RuntimeError("Failed to receive results array size")

    size = size_data[0]

    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise RuntimeError("Failed to receive full results data")
        data.extend(chunk)

    return list(data)


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

        elif tag == CONFIGURATION_SOCKET_TAG:
            result.append((CONFIGURATION_SOCKET_TAG, receive_config()))

        elif tag == BOARD_SOCKET_TAG:
            result.append((BOARD_SOCKET_TAG, receive_board()))

        elif tag == CONFIRMATION_SOCKET_TAG:
            result.append((CONFIRMATION_SOCKET_TAG, receive_confirmation()))

        elif tag == TURN_CHANGE_SOCKET_TAG:
            result.append((TURN_CHANGE_SOCKET_TAG, receive_turn_change()))

        elif tag == GAME_OVER_SOCKET_TAG:
            result.append((GAME_OVER_SOCKET_TAG, receive_game_over()))
        
        else: # Odebrano nieznany tag, socket najpewniej jest zawalony śmieciami z którymi nie wiemy co zrobić, nie możemy wydedukować stanu gry, trzeba zakończyć
            sock.close()
            raise RuntimeError("Received incorrect data")

        ready, _, _ = select.select([sock], [], [], 0)
        if not ready: # Jeśli nie ma więcej danych do pobrania kończymy
            return result

        tag = sock.recv(1)

    return result

def receive_next():
    """
    Odbiera jedną rzecz z socketa
    """
    result = (None, None)
    tag = sock.recv(1)
    if not tag:
        return result
    tag = struct.unpack('B', tag)[0]

    if tag == MAGIC_SOCKET_TAG:
        result = (MAGIC_SOCKET_TAG, receive_magic())

    elif tag == CONFIGURATION_SOCKET_TAG:
        result = (CONFIGURATION_SOCKET_TAG, receive_config())

    elif tag == BOARD_SOCKET_TAG:
        result = (BOARD_SOCKET_TAG, receive_board())

    elif tag == CONFIRMATION_SOCKET_TAG:
        result = (CONFIRMATION_SOCKET_TAG, receive_confirmation())

    elif tag == TURN_CHANGE_SOCKET_TAG:
        result = (TURN_CHANGE_SOCKET_TAG, receive_turn_change())

    elif tag == GAME_OVER_SOCKET_TAG:
        result = (GAME_OVER_SOCKET_TAG, receive_game_over())
    
    else: # Odebrano nieznany tag, socket najpewniej jest zawalony śmieciami z którymi nie wiemy co zrobić, nie możemy wydedukować stanu gry, trzeba zakończyć
        sock.close()
        raise RuntimeError("Received incorrect data")

    return result


# Klasa do budowy odpowiedzi wysyłanej przez AI
class ActionType:
    END_TURN = 0
    PLACE = 1
    MOVE = 2

class ActionBuilder:
    def __init__(self):
        self.buffer = bytearray()
        self.num = 0

    def add_place(self, unit_id: int, x: int, y: int):
        """
        Postawienie jednostki na polu o pozycji
        """
        self.buffer.append(ActionType.PLACE)
        self.buffer.append(unit_id)
        self.buffer.extend(struct.pack("!HH", x, y))
        num += 1
        if num == 255:
            self.send()

    def add_move(self, x_from: int, y_from: int, x_to: int, y_to: int):
        """
        Przesunięcie jednostki z pozycji na pozycję
        """
        self.buffer.append(ActionType.MOVE)
        self.buffer.extend(struct.pack("!HHHH", x_from, y_from, x_to, y_to))
        num += 1
        if num == 255:
            self.send()

    def add_end_turn(self):
        """
        Zakończenie tury, wstawienie tego od razu wywołuje send() (bo już nic dalej nie można zrobić)
        """
        self.buffer.append(ActionType.END_TURN)
        return self.send()

    def send(self):
        """
        Wysyła wszystkie ruchy jako jeden pakiet
        """
        if not self.buffer:
            return

        sock.sendall(bytes([ACTION_SOCKET_TAG]))
        sock.sendall(bytes([self.num]))
        sock.sendall(self.buffer)

        self.buffer.clear()



print("Started!")

# Program odpalany przez std::system("start python receiver.py 127.0.0.1 2137"); (nazwa, adres i port pochodzą z config.txt)
HOST = '127.0.0.1'
PORT = 2137

if len(sys.argv) >= 2:
    HOST = sys.argv[1]
if len(sys.argv) >= 3:
    PORT = int(sys.argv[2])


sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    sock.connect((HOST, PORT))
    while True:
        tag, payload = receive_next() # Czeka na dane

        if not tag: # Jeśli nie otrzymamy danych
            print("Server disconnected")
            input()
            break

        if tag == MAGIC_SOCKET_TAG:
            if payload:
                print("Correct magic numbers!")
            else:
                print("Wrong magic numbers!")
        
        elif tag == CONFIGURATION_SOCKET_TAG:
            print("Configuration received:")
            print(payload)

        elif tag == CONFIRMATION_SOCKET_TAG:
            approved, awaiting = payload
            print("Confirmation:", approved, awaiting)

        elif tag == BOARD_SOCKET_TAG:
            print("New board:")
            print(payload)

except:
    print("Connection error")
    input()

if sock:
    sock.close()
