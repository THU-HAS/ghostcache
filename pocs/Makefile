out=./out

all: primitive cc fullset victim fullset_intra

# example: make primitive
%: smc_%.c
	mkdir -p ./$(out)
	$(CC) -static -O0 $^ -o $(out)/$@
	objdump -d --source-comment -Sr $(out)/$@ > $(out)/$@.s

# example: make run_primitive
run_%:
	taskset -c 0 $(out)/$(patsubst run_%,%,$@)

clean:
	rm -rf ./$(out)

.PHONY: all
