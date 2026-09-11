RANGE ?= master

.PHONY: build run dry-run clean help

help:
	@echo "make build            # cmake configure + build -> build/git-progressive"
	@echo "make dry-run [RANGE=master]  # print the plan, no commits"
	@echo "make run [RANGE=master]      # build progressive commits on a new branch"
	@echo "make clean            # remove build/"

build:
	cmake -S . -B build
	cmake --build build

dry-run: build
	./build/git-progressive $(RANGE) --dry-run

run: build
	./build/git-progressive $(RANGE)

clean:
	rm -rf build
