.PHONY: test clean
test:
	$(MAKE) -C tests check-all
clean:
	$(MAKE) -C tests clean
