# -BlockChain-Simulator-
Tarea de sistemas distribuidos

El programa funciona con la siguiente sintaxis:

---
Servidor:

`./servidor <puerto> <mensaje_base> <num_ceros> <rango_por_cliente>`

Donde

`<puerto>` → Puerto TCP donde escuchará el servidor.

<mensaje_base> → Texto base al que se agregan los nonces.

<num_ceros> → Cantidad de ceros iniciales que debe tener al final el hash buscado.

<rango_por_cliente> → Cantidad de nonces que se asignan a cada cliente por solicitud.

---

Cliente:

./cliente <ip_servidor> <puerto> <espera>

Donde

<ip_servidor> → Dirección IP o localhost si está en la misma máquina.

<puerto> → Puerto donde está escuchando el servidor.

<espera> → tiempo de espera en milisegundos que hace el cliente en cada iteracion (para hacer pruebas).

el archivo run.sh es un ejecutable para hacer una prueba con un solo comando, teniendo los comandos seleccionados.

# -Uso del programa-
Teniendo todos los archivos en la misma carpeta, inicialmente se compilaran el servidor y el cliente, abriendo una consola en la misma carpeta ejecutamos:
```bash
gcc servidor.c -o servidor -lpthread -lcrypto
```
```bash
gcc cliente.c  -o cliente  -lcrypto
```

y podemos arrancar el programa con el comando:

```bash
bash run.sh
```

En caso de querer cambiar los parametros, se puede modificar el `run.sh`

# -Flujo del programa-
El Servidor arranca y queda esperando que un cliente se conecte, al conectarse, le envia el mensaje, la cantidad de ceros y el rango de relleno, y sigue esperando a que se conecten mas clientes hasta que reciba la respuesta de que ya fue completada la tarea, despues les manda un mensaje a los clientes para que se detengan.

El cliente por su parte, se intenta conectar al servidor, al conectarse, comienza la tarea probando los rellenos con la cadena de texto, si termina el rango, le dice al servidor que no encontro el relleno y recibe otro rango, si encuentra el relleno que cumpla la condicion, le responde al servidor con el hash y con el relleno; y es posible que el servidor le envie la orden de detenerse pues ya ha sido encontrado el hash.




