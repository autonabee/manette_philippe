import os
import json
import tkinter as tk
from tkinter import ttk
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

class TerminalEmulator:
    def __init__(self, root):
        self.root = root
        self.root.title("Emulateur VT100")

        # Zone de texte pour les caractères reçus
        self.text_area = tk.Text(root, height=10, width=50)
        self.text_area.pack(pady=5)

        # Champ de saisie
        self.entry = tk.Entry(root, width=50)
        self.entry.pack(pady=5)
        self.entry.bind("<Return>", self.send_data)

        # Liste déroulante pour le port série
        self.port_var = tk.StringVar()
        self.port_menu = ttk.Combobox(root, textvariable=self.port_var)
        self.port_menu['values'] = self.get_serial_ports()
        self.port_menu.pack(pady=5)

        # Bouton de connexion/déconnexion
        self.connect_button = tk.Button(root, text="Connect", command=self.connect_serial)
        self.connect_button.pack(pady=5)

        # Chargement des paramètres de configuration
        init_serial_config()  # Vérifier et initialiser le fichier JSON si nécessaire
        self.config = load_config()
        self.serial_conn = None
        self.port_var.set(self.config["last_port"])

    def get_serial_ports(self):
        return [port.device for port in serial.tools.list_ports.comports()]

    def save_config(self):
        """ Enregistre la dernière sélection de port série. """
        self.config["last_port"] = self.port_var.get()
        with open(CONFIG_FILE, "w") as file:
            json.dump(self.config, file, indent=4)

    def connect_serial(self):
        if self.serial_conn:
            self.serial_conn.close()
            self.serial_conn = None
            self.connect_button.config(text="Connect")
        else:
            try:
                self.serial_conn = serial.Serial(
                    self.port_var.get(),
                    baudrate=self.config["baudrate"],
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE,
                    timeout=1
                )
                self.serial_conn.setDTR(self.config["dtr"])  # Activation de DTR
                self.connect_button.config(text="Disconnect")
                self.root.after(100, self.read_serial)
                self.save_config()  # Sauvegarde du dernier port utilisé
            except Exception as e:
                print(f"Erreur de connexion: {e}")

    def read_serial(self):
        if self.serial_conn and self.serial_conn.is_open:
            data = self.serial_conn.read(1024).decode(errors="ignore")
            if data:
                self.text_area.insert(tk.END, data)
                self.text_area.see(tk.END)
            self.root.after(100, self.read_serial)

    def send_data(self, event):
        if self.serial_conn and self.serial_conn.is_open:
            data = self.entry.get() + "\r\n"
            self.serial_conn.write(data.encode())
            self.entry.delete(0, tk.END)

if __name__ == "__main__":
    root = tk.Tk()
    app = TerminalEmulator(root)
    root.mainloop()
