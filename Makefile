# For Amuc - the A'dam Music Composer

AMUC_DIR=src
A2PS_DIR=src-abcm2ps
W2S_DIR=src-wav2score
TRSCO_DIR=src-tr-sco
SDIR=/usr/share
INSTALL_DIR=$(SDIR)/amuc
BIN_DIR=/usr/bin
DOC_DIR=$(SDIR)/doc/amuc
# sometimes 'sudo make install' does not evaluate $(PWD) correctly, thus:
PWD=$(shell pwd)

.SUFFIXES=
.PHONY: all a2ps_lib wav2score tr-sco install links uninstall

all: a2ps_lib amuc wav2score tr-sco

a2ps_lib:
	make -C $(A2PS_DIR)

amuc:
	make -C $(AMUC_DIR)

wav2score:
	make -C $(W2S_DIR)

tr-sco:
	make -C $(TRSCO_DIR)

install:
	if test -e /usr/local/bin/amuc; then make uninstall; fi
	if test -L $(BIN_DIR)/amuc; then make uninstall; fi
	if test ! -d $(INSTALL_DIR); then mkdir $(INSTALL_DIR); fi
	if test ! -d $(DOC_DIR); then mkdir $(DOC_DIR); fi
	cp $(AMUC_DIR)/amuc $(BIN_DIR)/amuc
	cp $(A2PS_DIR)/abcm2ps $(BIN_DIR)/abcm2ps
	cp $(W2S_DIR)/wav2score $(BIN_DIR)/wav2score
	cp $(TRSCO_DIR)/tr-sco $(BIN_DIR)/tr-sco
	strip $(BIN_DIR)/amuc $(BIN_DIR)/wav2score $(BIN_DIR)/abcm2ps
	rm -rf $(INSTALL_DIR)/samples; cp -r samples $(INSTALL_DIR)
	cp tunes/monosynth-patches $(INSTALL_DIR)
	cp tunes/chords-and-scales $(INSTALL_DIR)
	cp doc/amuc.1 $(SDIR)/man/man1
	cp doc/* $(DOC_DIR)

# create links for executables
links:
	if test ! -d $(INSTALL_DIR); then mkdir $(INSTALL_DIR); fi
	ln -sf $(PWD)/$(AMUC_DIR)/amuc $(BIN_DIR)/amuc
	ln -sf $(PWD)/$(A2PS_DIR)/abcm2ps $(BIN_DIR)/abcm2ps
	ln -sf $(PWD)/$(W2S_DIR)/wav2score $(BIN_DIR)/wav2score
	ln -sf $(PWD)/$(TRSCO_DIR)/wav2score $(BIN_DIR)/tr-sco

uninstall:
	rm -f $(BIN_DIR)/amuc $(BIN_DIR)/abcm2ps $(BIN_DIR)/wav2score
	rm -rf $(INSTALL_DIR)
	rm -f $(SDIR)/man/man1/amuc.1
	rm -rf $(DOC_DIR)
