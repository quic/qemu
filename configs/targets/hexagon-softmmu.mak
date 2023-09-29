# Default configuration for hexagon-softmmu

TARGET_ARCH=hexagon
TARGET_SUPPORTS_MTTCG=y
TARGET_XML_FILES=gdb-xml/hexagon-core.xml gdb-xml/hexagon-hvx.xml gdb-xml/hexagon-sys.xml
CONFIG_SEMIHOSTING=y
CONFIG_ARM_COMPATIBLE_SEMIHOSTING=y

# NEEDSWORK: arm-compatible semihosting expects libc to open the default
# streams using the special pathname ":tt"
# (see https://github.com/ARM-software/abi-aa/blob/main/semihosting/semihosting.rst#sys-open-0x01).
# But Hexagon libc doesn't do that yet so, as a workaround, we need QEMU
# to open 0, 1, and 2 by default for the Hexagon applications.
CONFIG_SEMIHOSTING_USE_STDIO_FDS=y
