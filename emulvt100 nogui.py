import os
import json
import serial
import serial.tools.list_ports

CONFIG_FILE = os.path.join(os.path.dirname(__file__), "emul.json")

def init_serial_config():
    """ Initialise le fichier de configuration avec des valeurs par défaut si nécessaire. """
    default_config = {
        "baudrate": 9600,
        "parity": "N",
        "bytesize": 8,
        "stopbits": 1,
        "dtr": True,
        "last_port": ""
    }

    if not os.path.exists(CONFIG_FILE) or os.stat(CONFIG_FILE).st_size == 0:
        with open(CONFIG_FILE, "w") as file:
            json.dump(default_config, file, indent=4)
        print("Fichier emul.json créé avec les valeurs par défaut.")
    else:
        try:
            with open(CONFIG_FILE, "r") as file:
                config = json.load(file)
                if not isinstance(config, dict) or any(key not in config for key in default_config.keys()):
                    raise ValueError("Configuration invalide, réinitialisation nécessaire.")
        except (json.JSONDecodeError, ValueError):
            with open(CONFIG_FILE, "w") as file:
                json.dump(default_config, file, indent=4)
            print("Fichier emul.json corrompu, réinitialisé avec les valeurs par défaut.")

def load_config():
    """ Charge la configuration depuis le fichier JSON. """
    with open(CONFIG_FILE, "r") as file:
        return json.load(file)

def get_serial_ports():
    """ Retourne la liste des ports série disponibles. """
    return [port.device for port in serial.tools.list_ports.comports()]

def connect_serial(port):
    """ Ouvre la connexion série en appliquant la configuration. """
    config = load_config()
    try:
        ser = serial.Serial(
            port,
            baudrate=config["baudrate"],
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1
        )
        ser.setDTR(config["dtr"])  # Activation de DTR
        print(f"Connecté à {port} avec configuration {config}")
        return ser
    except Exception as e:
        print(f"Erreur de connexion : {e}")
        return None

def terminal_loop(ser):
    """ Boucle principale permettant l'interaction avec le port série. """
    try:
        while True:
            user_input = input(">>> ")  # Entrée utilisateur
            if user_input.lower() == "exit":
                break
            ser.write((user_input + "\r\n").encode())  # Envoi de la ligne avec CRLF
            data = ser.read(1024).decode(errors="ignore")  # Lecture des données reçues
            if data:
                print(f"Réponse: {data.strip()}")  # Affichage des données reçues

    except KeyboardInterrupt:
        print("\nFermeture du terminal.")

    finally:
        ser.close()
        print("Connexion série fermée.")

if __name__ == "__main__":
    init_serial_config()
    ports = get_serial_ports()
    
    if not ports:
        print("Aucun port série détecté.")
    else:
        print("Ports disponibles :", ports)
        selected_port = input("Sélectionnez un port série : ")
        serial_conn = connect_serial(selected_port)

        if serial_conn:
            terminal_loop(serial_conn)