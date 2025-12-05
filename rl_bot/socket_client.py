"""
Socket client for connecting to Antiyoy game server.
Handles binary protocol, state exchange, and action encoding.
"""

import socket
import struct
import logging
from enum import IntEnum
from typing import Dict, Tuple, Optional

logging.basicConfig(level=logging.WARNING)
logger = logging.getLogger(__name__)


# Socket protocol tags
class SocketTag(IntEnum):
    MAGIC = 0
    CONFIGURATION = 1
    BOARD = 2
    ACTION = 3
    CONFIRMATION = 4
    TURN_CHANGE = 5
    GAME_OVER = 6
    VALID_ACTIONS = 7


# Resident types (must match C++ enum)
class Resident(IntEnum):
    WATER = 0
    EMPTY = 1
    WARRIOR1 = 2
    WARRIOR2 = 3
    WARRIOR3 = 4
    WARRIOR4 = 5
    WARRIOR1_MOVED = 6
    WARRIOR2_MOVED = 7
    WARRIOR3_MOVED = 8
    WARRIOR4_MOVED = 9
    FARM = 10
    CASTLE = 11
    TOWER = 12
    STRONG_TOWER = 13
    PALM_TREE = 14
    PINE_TREE = 15
    GRAVESTONE = 16


MAGIC_NUMBERS = [ord(c) for c in "ANTIYOY"]


class GameSocketClient:
    """
    Client for communicating with Antiyoy C++ game via TCP sockets.
    """

    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.sock = None
        self.board_width = 0
        self.board_height = 0
        self.current_player_id = 1
        self.game_config = {}

    def connect(self) -> bool:
        """Connect to game server."""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            logger.info(f"Connected to {self.host}:{self.port}")
            return True
        except Exception as e:
            logger.error(f"Connection failed: {e}")
            return False

    def disconnect(self):
        """Close socket connection."""
        if self.sock:
            self.sock.close()
            self.sock = None

    def receive_magic_numbers(self) -> bool:
        """Receive magic numbers from server (game sends them first)."""
        try:
            data = self._recv_exact(1 + len(MAGIC_NUMBERS))
            tag = data[0]
            magic = list(data[1:])
            
            if tag != SocketTag.MAGIC or magic != MAGIC_NUMBERS:
                logger.error(f"Magic number mismatch! Got tag={tag}, magic={magic}")
                return False
            
            logger.info("Magic numbers received from server")
            return True
        except Exception as e:
            logger.error(f"Failed to receive magic numbers: {e}")
            return False
    
    def close(self):
        """Close the socket connection."""
        if self.sock:
            try:
                self.sock.close()
            except Exception as e:
                logger.error(f"Error closing socket: {e}")
            finally:
                self.sock = None
    
    def send_magic_numbers(self) -> bool:
        """Send magic numbers back to server (response to server's magic numbers)."""
        try:
            data = bytes([SocketTag.MAGIC] + MAGIC_NUMBERS)
            self.sock.sendall(data)
            logger.info("Sent magic numbers back to server")
            return True
        except Exception as e:
            logger.error(f"Failed to send magic numbers: {e}")
            return False

    def receive_game_config(self, expect_tag: bool = True) -> bool:
        """
        Receive game configuration.
        Returns: (board_width, board_height, seed, min_size, max_size, player_markers)
        """
        try:
            logger.debug(f"receive_game_config: expect_tag={expect_tag}")
            
            if expect_tag:
                logger.debug("Waiting for CONFIGURATION tag...")
                tag_byte = self._recv_exact(1)[0]
                logger.debug(f"Received tag byte: {tag_byte}")
                if tag_byte != SocketTag.CONFIGURATION:
                    logger.error(f"Expected CONFIGURATION tag (1), got {tag_byte}")
                    return False
                logger.debug("Tag verified, reading config data...")
            
            # Read config data (following C++ GameConfigData structure)
            logger.debug("Reading x...")
            x_bytes = self._recv_exact(2)
            logger.debug(f"  x: {list(x_bytes)}")
            
            logger.debug("Reading y...")
            y_bytes = self._recv_exact(2)
            logger.debug(f"  y: {list(y_bytes)}")
            
            logger.debug("Reading seed...")
            seed_bytes = self._recv_exact(4)
            logger.debug(f"  seed: {list(seed_bytes)}")
            
            logger.debug("Reading min...")
            min_bytes = self._recv_exact(4)
            logger.debug(f"  min: {list(min_bytes)}")
            
            logger.debug("Reading max...")
            max_bytes = self._recv_exact(4)
            logger.debug(f"  max: {list(max_bytes)}")
            
            x = struct.unpack(">H", x_bytes)[0]  # Big-endian uint16
            y = struct.unpack(">H", y_bytes)[0]
            seed = struct.unpack(">I", seed_bytes)[0]  # Big-endian uint32
            min_size = struct.unpack(">I", min_bytes)[0]
            max_size = struct.unpack(">I", max_bytes)[0]
            
            logger.debug(f"Board: {x}x{y}, seed={seed}, min={min_size}, max={max_size}")
            
            # Read player markers
            logger.debug("Reading marker_len...")
            marker_len = self._recv_exact(1)[0]
            logger.debug(f"  marker_len: {marker_len}")
            
            logger.debug("Reading markers...")
            markers = self._recv_exact(marker_len).decode('utf-8')
            logger.debug(f"  markers: {markers}")
            
            # Read max move times
            logger.debug("Reading move_times_len...")
            move_times_len = self._recv_exact(1)[0]
            logger.debug(f"  move_times_len: {move_times_len}")
            
            move_times = []
            for i in range(move_times_len):
                logger.debug(f"  Reading move_time[{i}]...")
                mt_bytes = self._recv_exact(4)
                mt = struct.unpack(">I", mt_bytes)[0]
                logger.debug(f"    move_time[{i}]: {mt}")
                move_times.append(mt)
            
            self.board_width = x
            self.board_height = y
            self.game_config = {
                "width": x,
                "height": y,
                "seed": seed,
                "min_province_size": min_size,
                "max_province_size": max_size,
                "player_markers": markers,
                "max_move_times": move_times
            }
            
            logger.info(f"Game config received: {x}x{y}, players={markers}")
            return True
        except Exception as e:
            logger.error(f"Failed to receive game config: {e}")
            return False

    def receive_board_state(self) -> Optional[Dict]:
        """
        Receive board state from server.
        C++ sends: tag(1) + width(2) + height(2) + hex_data(width*height*2)
        Returns dict mapping (x, y) to (owner_id, resident) or None on error.
        """
        try:
            tag = self._recv_exact(1)[0]
            if tag != SocketTag.BOARD:
                logger.error(f"Expected BOARD tag, got {tag}")
                return None
            
            # Read width and height (big-endian uint16)
            width_bytes = self._recv_exact(2)
            height_bytes = self._recv_exact(2)
            width = struct.unpack(">H", width_bytes)[0]
            height = struct.unpack(">H", height_bytes)[0]
            
            logger.debug(f"Receiving board: {width}x{height}")
            
            # Read board data
            board_data = {}
            for y in range(height):
                for x in range(width):
                    owner_bytes = self._recv_exact(1)
                    resident_bytes = self._recv_exact(1)
                    
                    owner_id = owner_bytes[0]
                    resident_value = resident_bytes[0]
                    
                    # Handle invalid resident values gracefully
                    try:
                        resident = Resident(resident_value)
                    except ValueError:
                        logger.warning(f"Invalid resident value {resident_value} at ({x},{y}), using EMPTY")
                        resident = Resident.EMPTY
                    
                    board_data[(x, y)] = {
                        "owner_id": owner_id,
                        "resident": resident.name.lower()
                    }
            
            logger.debug(f"Board state received: {width}x{height}, {len(board_data)} hexes")
            return board_data
        except Exception as e:
            logger.error(f"Failed to receive board state: {e}")
            return None

    def receive_turn_change(self) -> Optional[int]:
        """Receive next player ID."""
        try:
            tag = self._recv_exact(1)[0]
            if tag != SocketTag.TURN_CHANGE:
                logger.error(f"Expected TURN_CHANGE tag, got {tag}")
                return None
            
            player_id = self._recv_exact(1)[0]
            self.current_player_id = player_id
            logger.info(f"Turn changed to player {player_id}")
            return player_id
        except Exception as e:
            logger.error(f"Failed to receive turn change: {e}")
            return None

    def receive_confirmation(self) -> Optional[Tuple[bool, bool]]:
        """
        Receive move confirmation.
        Returns: (approved: bool, awaiting: bool) or None on error.
        """
        try:
            tag = self._recv_exact(1)[0]
            if tag != SocketTag.CONFIRMATION:
                logger.error(f"Expected CONFIRMATION tag, got {tag}")
                return None
            
            approved = bool(self._recv_exact(1)[0])
            awaiting = bool(self._recv_exact(1)[0])
            
            logger.debug(f"Confirmation: approved={approved}, awaiting={awaiting}")
            return (approved, awaiting)
        except Exception as e:
            logger.error(f"Failed to receive confirmation: {e}")
            return None

    def send_action_end_turn(self) -> bool:
        """Send 'end turn' action."""
        try:
            data = bytes([
                SocketTag.ACTION,  # Action tag
                1,                 # Action count
                0                  # End turn marker
            ])
            self.sock.sendall(data)
            logger.info("Sent: END_TURN")
            return True
        except Exception as e:
            logger.error(f"Failed to send end turn: {e}")
            return False

    def send_action_place(self, resident_type: int, from_x: int, from_y: int, 
                          to_x: int, to_y: int) -> bool:
        """Send 'place unit' action."""
        try:
            # Encode coordinates as big-endian uint16
            from_x_bytes = struct.pack(">H", from_x)
            from_y_bytes = struct.pack(">H", from_y)
            to_x_bytes = struct.pack(">H", to_x)
            to_y_bytes = struct.pack(">H", to_y)
            
            data = bytes([
                SocketTag.ACTION,  # Action tag
                1,                 # Action count
                1,                 # Place action type
                resident_type      # Resident enum value
            ]) + from_x_bytes + from_y_bytes + to_x_bytes + to_y_bytes
            
            self.sock.sendall(data)
            logger.info(f"Sent: PLACE resident={resident_type} ({from_x},{from_y})->({to_x},{to_y})")
            return True
        except Exception as e:
            logger.error(f"Failed to send place action: {e}")
            return False

    def send_action_move(self, from_x: int, from_y: int, to_x: int, to_y: int) -> bool:
        """Send 'move warrior' action."""
        try:
            from_x_bytes = struct.pack(">H", from_x)
            from_y_bytes = struct.pack(">H", from_y)
            to_x_bytes = struct.pack(">H", to_x)
            to_y_bytes = struct.pack(">H", to_y)
            
            data = bytes([
                SocketTag.ACTION,  # Action tag
                1,                 # Action count
                2                  # Move action type
            ]) + from_x_bytes + from_y_bytes + to_x_bytes + to_y_bytes
            
            self.sock.sendall(data)
            logger.info(f"Sent: MOVE ({from_x},{from_y})->({to_x},{to_y})")
            return True
        except Exception as e:
            logger.error(f"Failed to send move action: {e}")
            return False

    def request_valid_actions(self) -> list:
        """
        Request valid actions from the server.
        Returns a list of raw action tuples:
        - Place: ('place', resident, x, y, to_x, to_y)
        - Move: ('move', from_x, from_y, to_x, to_y)
        """
        if not self.sock:
            return []
            
        try:
            # Send request tag
            self.sock.send(struct.pack('b', SocketTag.VALID_ACTIONS))
            
            # Receive response tag
            tag = self.sock.recv(1)
            if not tag or tag[0] != SocketTag.VALID_ACTIONS:
                logger.error(f"Expected VALID_ACTIONS tag, got {tag}")
                return []
                
            # Receive count (2 bytes)
            count_bytes = self.sock.recv(2)
            if not count_bytes:
                return []
            count = struct.unpack('!H', count_bytes)[0]
            
            actions = []
            for _ in range(count):
                # Receive action type
                type_byte = self.sock.recv(1)
                if not type_byte:
                    break
                action_type = type_byte[0]
                
                if action_type == 1: # Place
                    # Resident (1), FromX(2), FromY(2), ToX(2), ToY(2)
                    data = self.sock.recv(9)
                    if len(data) < 9: break
                    resident = data[0]
                    fx = struct.unpack('!H', data[1:3])[0]
                    fy = struct.unpack('!H', data[3:5])[0]
                    tx = struct.unpack('!H', data[5:7])[0]
                    ty = struct.unpack('!H', data[7:9])[0]
                    actions.append(('place', resident, fx, fy, tx, ty))
                    
                elif action_type == 2: # Move
                    # FromX(2), FromY(2), ToX(2), ToY(2)
                    data = self.sock.recv(8)
                    if len(data) < 8: break
                    fx = struct.unpack('!H', data[0:2])[0]
                    fy = struct.unpack('!H', data[2:4])[0]
                    tx = struct.unpack('!H', data[4:6])[0]
                    ty = struct.unpack('!H', data[6:8])[0]
                    actions.append(('move', fx, fy, tx, ty))
            
            return actions
            
        except Exception as e:
            logger.error(f"Error requesting valid actions: {e}")
            return []

    def _recv_exact(self, nbytes: int, timeout: float = 10.0) -> bytes:
        """Receive exactly nbytes bytes from socket with timeout."""
        self.sock.settimeout(timeout)
        
        data = b""
        retries = 0
        max_retries = 3
        
        try:
            while len(data) < nbytes:
                try:
                    chunk = self.sock.recv(nbytes - len(data))
                    if not chunk:
                        raise RuntimeError(f"Socket closed, expected {nbytes} bytes, got {len(data)}")
                    data += chunk
                    retries = 0  # Reset retry counter on successful recv
                except socket.timeout:
                    if retries < max_retries:
                        # Retry with increased timeout
                        retries += 1
                        new_timeout = timeout * (2 ** retries)
                        logger.debug(f"Timeout (retry {retries}/{max_retries}), trying with {new_timeout}s...")
                        self.sock.settimeout(new_timeout)
                        continue
                    else:
                        raise RuntimeError(f"Socket timeout after {max_retries} retries waiting for {nbytes} bytes, got {len(data)}")
        except RuntimeError as e:
            raise e
        except Exception as e:
            raise RuntimeError(f"Socket error waiting for {nbytes} bytes: {e}")
        finally:
            # Reset to blocking mode
            self.sock.settimeout(None)
        
        return data


if __name__ == "__main__":
    # Test socket client
    client = GameSocketClient("127.0.0.1", 2137)
    if client.connect():
        client.send_magic_numbers()
        client.receive_magic_numbers()
        client.receive_game_config(expect_tag=True)
        
        board = client.receive_board_state()
        if board:
            print(f"Received board with {len(board)} hexagons")
        
        client.disconnect()
