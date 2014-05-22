#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux-x86
CND_DLIB_EXT=so
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/main/cli.o \
	${OBJECTDIR}/main/config.o \
	${OBJECTDIR}/main/css_monitor.o \
	${OBJECTDIR}/main/cssmm.o \
	${OBJECTDIR}/main/cssobj2.o \
	${OBJECTDIR}/main/cssplayer.o \
	${OBJECTDIR}/main/io.o \
	${OBJECTDIR}/main/localtime.o \
	${OBJECTDIR}/main/lock.o \
	${OBJECTDIR}/main/logger.o \
	${OBJECTDIR}/main/md5.o \
	${OBJECTDIR}/main/netsock2.o \
	${OBJECTDIR}/main/sha1.o \
	${OBJECTDIR}/main/strcompat.o \
	${OBJECTDIR}/main/strings.o \
	${OBJECTDIR}/main/sysloger.o \
	${OBJECTDIR}/main/term.o \
	${OBJECTDIR}/main/threadstorage.o \
	${OBJECTDIR}/main/utils.o


# C Compiler Flags
CFLAGS=-Wall -lm -g -L/usr/pkg/lib -levent -lrt -lpthread -lcrypto -ledit

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/css_player_server

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/css_player_server: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.c} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/css_player_server ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/include/css_monitor.h.gch: include/css_monitor.h 
	${MKDIR} -p ${OBJECTDIR}/include
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o "$@" include/css_monitor.h

${OBJECTDIR}/main/cli.o: main/cli.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/cli.o main/cli.c

${OBJECTDIR}/main/config.o: main/config.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/config.o main/config.c

${OBJECTDIR}/main/css_monitor.o: main/css_monitor.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/css_monitor.o main/css_monitor.c

${OBJECTDIR}/main/cssmm.o: main/cssmm.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/cssmm.o main/cssmm.c

${OBJECTDIR}/main/cssobj2.o: main/cssobj2.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/cssobj2.o main/cssobj2.c

${OBJECTDIR}/main/cssplayer.o: main/cssplayer.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/cssplayer.o main/cssplayer.c

${OBJECTDIR}/main/io.o: main/io.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/io.o main/io.c

${OBJECTDIR}/main/localtime.o: main/localtime.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/localtime.o main/localtime.c

${OBJECTDIR}/main/lock.o: main/lock.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/lock.o main/lock.c

${OBJECTDIR}/main/logger.o: main/logger.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/logger.o main/logger.c

${OBJECTDIR}/main/md5.o: main/md5.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/md5.o main/md5.c

${OBJECTDIR}/main/netsock2.o: main/netsock2.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/netsock2.o main/netsock2.c

${OBJECTDIR}/main/sha1.o: main/sha1.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/sha1.o main/sha1.c

${OBJECTDIR}/main/strcompat.o: main/strcompat.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/strcompat.o main/strcompat.c

${OBJECTDIR}/main/strings.o: main/strings.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/strings.o main/strings.c

${OBJECTDIR}/main/sysloger.o: main/sysloger.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/sysloger.o main/sysloger.c

${OBJECTDIR}/main/term.o: main/term.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/term.o main/term.c

${OBJECTDIR}/main/threadstorage.o: main/threadstorage.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/threadstorage.o main/threadstorage.c

${OBJECTDIR}/main/utils.o: main/utils.c 
	${MKDIR} -p ${OBJECTDIR}/main
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main/utils.o main/utils.c

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/css_player_server

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
