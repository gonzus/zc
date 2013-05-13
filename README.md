ZC -- A C-based netcat using 0MQ
================================


What is does
------------

zc is command line interface that allows you to create a [0MQ][2] socket and use it as the reading or writing end of a pipeline.  It is inspired on [zmqc][1] but developed in C, so its only dependency is a proper working instance of the [0MQ library][3].


How to build
------------

Assuming you have installed the 0MQ library, you should be able to simply do:

    cd src
    make

and get the executable zc command.


How to use
----------

**To Be Completed**.  In the meantime, please check [zmqc][1].


[1]: https://github.com/zacharyvoase/zmqc   "zmqc"
[2]: http://www.zeromq.org/                 "0MQ"
[3]: https://github.com/zeromq/libzmq       "0MQ library"
