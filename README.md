# async-io
async-io was a asynchronous io library, by invoke kernel interface( io_setup io_submit and io_getevents) by syscall, and not depend libaio.

it provied some functionality  such as  read/write, and can set io depth. likely fio.

Building
--------

Just type::
make all

Platforms
---------

async-io  works on Linux(CentOS and ubuntu).
kernel version must greater than 2.6

