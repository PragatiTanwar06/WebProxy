Reads a request from a browser, forwards that request to a web server, reads the reply from the web server, and forwards the reply back to the browser.
Program also have a ‘blocking’ part which means web requests to certain black listed web sites will not be returned.

doParse() -- to scan and parse the browser request

doHTTP() -- to actually transfer the file