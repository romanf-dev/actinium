Drivers
=======


Being a microkernel Actinium supports usermode drivers. Driver tasks follow
the same rules as regular tasks except they have direct access to managed 
hardware peripherals.
Because the driver must interact with the rest of the system by async message
passing, driver development may be quite challenging. This doc describes basic
approaches to device driver development.


Leaf drivers
------------

The simplest form of the device driver is the one which just performs some 
action on behalf of a client and does not reply. These drivers receive messages
from clients but never produce messages so they are 'leafs' of message passing
tree. Examples are drivers that switch LEDs, set GPIO pins and so on.
Clients send their requests to a single dedicated request channel. The driver
subscribes to that channel, performs the action, then free the message.


Immediate response
------------------

The next level of complexity is drivers where some response is required, not 
just an action. Typically each client has its own response channel, a driver 
actor also maintains a single request channel. Calling of this type of driver
by a client consists of the following stages:

- driver accepts client request from the request channel
- driver processes request and extracts response channel id from the message
- driver frees the request message
- driver allocates response message (or reuses request message if possible)
- driver responds to the caller using response channel id
- driver subscribes to new requests

TODO: what about response id spoofing by malicious actors?


Delayed response
----------------

The 'direct response' scheme is universal enough even for relatively complex
applications but there is a catch: there are certain  situations when immediate 
response is not possible. For example this is the case when after accepting a 
message the driver has to wait for an interrupt from a device. In this case the 
driver should wait either interrupt or new request. Example is shown below:

- driver accepts a request from a client
- driver saves the info from the message then frees it
- driver programs the device and subscribes to the interrupts channel waiting for device response
- when interrupt arrives the driver frees the interrupt message
- driver allocates a new message for the response
- driver sends reply and starts waiting for request channel again


Driver reliability
------------------

There is a possibility of driver crash when it waits for interrupts or during
message processing after interrupt notification. So the driver does not hold 
request messages on crash. If driver crashes then it would not have any chance
to notify blocked clients.
The solution is to assign a dedicated channel to the driver where it may save
messages from outstanding requests temporarily. When the driver accepts a message
it moves it into the channel to make receiving of another messages possible.
Presence of messages in the private channel means that there are clients blocked on 
this driver so if the driver crashes it can examine the channel and act accordingly 
after restart.


Asynchronous devices
--------------------

The most complex case is devices like UARTs or network adapters. These
devices are asynchronous so there are two possible reasons of driver activation:
either requests from clients or asychronous activity like receiving the next packet 
from the network. The latter is unpredictable.
One possible solution is to use request channel not only for client requests but also
for interrupt notification. Then driver would be activated on any type of request.
But this design has unobvious flaw: when the driver has programmed the device and 
tries to wait for interrupts using the same request channel then requests from other
clients may be enqueued before interrupt notification message. This may lead to 
deadlocks for some types of devices and protocols. The solution is the same: use the
dedicated private channel to save client requests and extract as many messages from
the request channel as needed to complete the current active request. Then extract
the next user request synchronously from the private channel.

The rule of thumb: if your device has to respond with interrupts always and you
always know what are you wait for then use dedicated interrupt channel. Otherwise
use the design with shared channel for requests and interrupts and perform the 
necessary bookkeeping inside the driver, possibly using private channels to save
messages temporatily during processing.

