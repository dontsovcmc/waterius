Import("env", "projenv")

env.AddPostAction(
	"$BUILD_DIR/${PROGNAME}.elf",
	env.VerboseAction("avr-objdump -d $BUILD_DIR/${PROGNAME}.elf > $BUILD_DIR/${PROGNAME}.lss", 
	"Creating $BUILD_DIR/${PROGNAME}.lss")
)
