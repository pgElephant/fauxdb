#if __INTEL_LLVM_COMPILER < 1000000L
#define COMPILER_VERSION_MAJOR DEC(__INTEL_LLVM_COMPILER / 100)
#define COMPILER_VERSION_MINOR DEC(__INTEL_LLVM_COMPILER / 10 % 10)
#define COMPILER_VERSION_PATCH DEC(__INTEL_LLVM_COMPILER % 10)
#else
#define COMPILER_VERSION_MAJOR DEC(__INTEL_LLVM_COMPILER / 10000)
#define COMPILER_VERSION_MINOR DEC(__INTEL_LLVM_COMPILER / 100 % 100)
#define COMPILER_VERSION_PATCH DEC(__INTEL_LLVM_COMPILER % 100)
#endif
#if defined(_MSC_VER)
#if defined(_WIN32) && defined(_MSC_VER)
#if defined(_M_IA64)
#define ARCHITECTURE_ID "IA64"

#elif defined(_M_ARM64EC)
#define ARCHITECTURE_ID "ARM64EC"

#elif defined(_M_X64) || defined(_M_AMD64)
#define ARCHITECTURE_ID "x64"

#elif defined(_M_IX86)
#define ARCHITECTURE_ID "X86"

#elif defined(_M_ARM64)
#define ARCHITECTURE_ID "ARM64"

#elif defined(_M_ARM)
#if _M_ARM == 4
#define ARCHITECTURE_ID "ARMV4I"
#elif _M_ARM == 5
#define ARCHITECTURE_ID "ARMV5I"
#else
#define ARCHITECTURE_ID "ARMV" STRINGIFY(_M_ARM)
#endif

#elif defined(_M_MIPS)
#define ARCHITECTURE_ID "MIPS"

#elif defined(_M_SH)
#define ARCHITECTURE_ID "SHx"

#else /* unknown architecture */
#define ARCHITECTURE_ID ""
#endif

#elif defined(__WATCOMC__)
#if defined(_M_I86)
#define ARCHITECTURE_ID "I86"

#elif defined(_M_IX86)
#define ARCHITECTURE_ID "X86"

#else /* unknown architecture */
#define ARCHITECTURE_ID ""
#endif

#elif defined(__IAR_SYSTEMS_ICC__) || defined(__IAR_SYSTEMS_ICC)
#if defined(__ICCARM__)
#define ARCHITECTURE_ID "ARM"

#elif defined(__ICCRX__)
#define ARCHITECTURE_ID "RX"

#elif defined(__ICCRH850__)
#define ARCHITECTURE_ID "RH850"

#elif defined(__ICCRL78__)
#define ARCHITECTURE_ID "RL78"

#elif defined(__ICCRISCV__)
#define ARCHITECTURE_ID "RISCV"

#elif defined(__ICCAVR__)
#define ARCHITECTURE_ID "AVR"

#elif defined(__ICC430__)
#define ARCHITECTURE_ID "MSP430"

#elif defined(__ICCV850__)
#define ARCHITECTURE_ID "V850"

#elif defined(__ICC8051__)
#define ARCHITECTURE_ID "8051"

#elif defined(__ICCSTM8__)
#define ARCHITECTURE_ID "STM8"

#else /* unknown architecture */
#define ARCHITECTURE_ID ""
#endif

#elif defined(__ghs__)
#if defined(__PPC64__)
#define ARCHITECTURE_ID "PPC64"

#elif defined(__ppc__)
#define ARCHITECTURE_ID "PPC"

#elif defined(__ARM__)
#define ARCHITECTURE_ID "ARM"

#elif defined(__x86_64__)
#define ARCHITECTURE_ID "x64"

#elif defined(__i386__)
#define ARCHITECTURE_ID "X86"

#else /* unknown architecture */
#define ARCHITECTURE_ID ""
#endif

#elif defined(__clang__) && defined(__ti__)
#if defined(__ARM_ARCH)
#define ARCHITECTURE_ID "ARM"

#else /* unknown architecture */
#define ARCHITECTURE_ID ""
#endif

#elif defined(__TI_COMPILER_VERSION__)
#if defined(__TI_ARM__)
#define ARCHITECTURE_ID "ARM"

#elif defined(__MSP430__)
#define ARCHITECTURE_ID "MSP430"

#elif defined(__TMS320C28XX__)
#define ARCHITECTURE_ID "TMS320C28x"

#elif defined(__TMS320C6X__) || defined(_TMS320C6X)
#define ARCHITECTURE_ID "TMS320C6x"

#else /* unknown architecture */
#define ARCHITECTURE_ID ""
#endif

#elif defined(__ADSPSHARC__)
#define ARCHITECTURE_ID "SHARC"

#elif defined(__ADSPBLACKFIN__)
#define ARCHITECTURE_ID "Blackfin"

#elif defined(__TASKING__)

#if defined(__CTC__) || defined(__CPTC__)
#define ARCHITECTURE_ID "TriCore"

#elif defined(__CMCS__)
#define ARCHITECTURE_ID "MCS"

#elif defined(__CARM__) || defined(__CPARM__)
#define ARCHITECTURE_ID "ARM"

#elif defined(__CARC__)
#define ARCHITECTURE_ID "ARC"

#elif defined(__C51__)
#define ARCHITECTURE_ID "8051"

#elif defined(__CPCP__)
#define ARCHITECTURE_ID "PCP"

#else
#define ARCHITECTURE_ID ""
#endif

#elif defined(__RENESAS__)
#if defined(__CCRX__)
#define ARCHITECTURE_ID "RX"

#elif defined(__CCRL__)
#define ARCHITECTURE_ID "RL78"

#elif defined(__CCRH__)
#define ARCHITECTURE_ID "RH850"

#else
#define ARCHITECTURE_ID ""
#endif

#else
#define ARCHITECTURE_ID
#endif

