# Included from ../Makefile; paths are relative to src/.

SHARED_SERVICE_OBJS := shared_service/blob_service.o
SHARED_SERVICE_RELOC_CHECK := shared_service/blob_service.reloc-ok

shared_service/blob_service.o: shared_service/blob_service.c
	$(COMPILE_C)

$(SHARED_SERVICE_RELOC_CHECK): shared_service/blob_service.o $(BUILD_SCRIPTS)/check_blobsvc_reloc.py
	python3 $(BUILD_SCRIPTS)/check_blobsvc_reloc.py $<
	touch $@
