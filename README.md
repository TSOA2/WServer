# WServer

## What is this?
This is an HTTP(1.1) server that I put together over a week or so. It's very much not finished and has a couple errors, but I got busy with school. The goal for this project wasn't to be fast or robust (although if you look at the code you can see hints of micro-optimization), but was to learn more about HTTP and have fun.

### What it can do!
* According to `wrk`, serve ~110k reqs/sec on my MacBook (through localhost of course)
* Parse paths and return corresponding files/resources
* Log requests

### Limitations
* Not robust or safe, various bugs with `kqueue` that I have yet to solve
* Incorrectly handles file requests; ex. if a file is created while the server is running, the server won't recognize it
* Stores configuration in a header file
* Not portable: only works for BSD systems / MacOS

And many more.

### Result
I had fun with this, and I think it was great to learn more about HTTP. I definitely think I could have done better with code structure, and I might come back to this project in the future or just start a fresh redo of the whole thing.
