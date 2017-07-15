# Using env variable DBG, see ../src/Makefile
CC = g++
ADIR=../src
OBJS=$(ADIR)/str.o

.SUFFIXES=

tr-sco: $(OBJS) tr-sco.cpp
	$(CC) -O $(DBG) -o tr-sco -I$(ADIR) tr-sco.cpp $(OBJS)

$(ADIR)/%.o: $(ADIR)/%.cpp $(ADIR)/%.h
	make -C $(ADIR) $(@F)
