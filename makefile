all: server client

server: server.c
    gcc -o tsamgroup43 server.c

client: client.c
    gcc -o client client.c

clean:
    rm -f tsamgrou43 client