all:
	$(MAKE) -C src

test:
	$(MAKE) -C src test

start:
	$(MAKE) -C src start

clean:
	$(MAKE) -C src clean
