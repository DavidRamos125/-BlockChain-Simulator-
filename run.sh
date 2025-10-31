#!/bin/bash
# arranca el servidor en una terminal
gnome-terminal -- bash -c "./servidor 3333 paranguaricutirimicuaro 3 30; exec bash"

# Esperar un poco para que el servidor arranque
sleep 1

# arranca los clientes en consolas separadas
gnome-terminal -- bash -c "./cliente 127.0.0.1 3333 10; exec bash"
gnome-terminal -- bash -c "./cliente 127.0.0.1 3333 10; exec bash"
gnome-terminal -- bash -c "./cliente 127.0.0.1 3333 10; exec bash"
