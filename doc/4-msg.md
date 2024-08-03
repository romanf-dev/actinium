Reliable communication
======================

Let's consider simple 'read file' example when client requests data from a
server. The example use pair of channels: one for requests and one for 
replies. Initially reply channel contains a **single** message available for
allocation by the client, request channel is empty.


                CLIENT ACTOR                       SERVER ACTOR

        +----------------------------+
        | get msg from reply channel |<--+
        +----------------------------+   |
                       | (1)             |
                       V                 |
        +----------------------------+   |
        |    set request payload     |   |
        +----------------------------+   |              +-------------+
                       |                 |              |             |
                       V                 |              V             |
        +----------------------------+   |    +-------------------+   |
        |  send msg to req channel   |------->| wait for requests |   |
        +----------------------------+   |    +-------------------+   |
                       | (2)             |              | (1)         |
                       |                 |              V             |
                       |                 |    +-------------------+   |
                       |                 |    |   handle request  |   |
                       |                 |    +-------------------+   |
                       |                 |              |             |
                       V                 |              V             |
        +----------------------------+   |    +-------------------+   |
        | wait for reply channel     |<-------|    send reply     |   |
        +----------------------------+   |    +-------------------+   |
                       | (3)             |              | (2)         |
                       V                 |              +-------------+
        +----------------------------+   |
        |        poisoned msg ?      |---+ Yes,
        +----------------------------+     free msg and restart
                       |
                       V No


When the client crashes at:
1.  Message returned to the channel, server is unaware, after client's restart
    we're at the initial point again.
2.  Client has successfully sent the message, then crashed. After request is
    handled by the server the message will be returned into reply channel and
    client retries its request after restart.
3.  Crash after full request-reply sequence. Client will retry its request 
    after restart. Message will be returned to the pool after crash and is
    available for allocation again.

When the server crashes at:
1.  Server crashed during request processing. Message is freed by the system as
    'poisoned'. If client is blocked at (1) it will proceed normally, 
    if it is blocked at (3) it will retry the request because of poisoned reply.
2.  Server crashes after reply: it does not own messages, clients are 
    unaffected.

So, after arbitrary IPC participant is crashed at any point the system has 
ability to recover its state and proceed.
Stateful servers require some 'connect' initial message from clients so
they would be able to clear internal state in case of client crash.

