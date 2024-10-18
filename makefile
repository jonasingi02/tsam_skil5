all: server client

server: server.c
    gcc -o tsamgroup1 server.c

client: client.c
    gcc -o client client.c

clean:
    rm -f tsamgroup1 client