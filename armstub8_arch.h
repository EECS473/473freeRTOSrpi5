#ifndef ARMSTUB8_ARCH_H_
#define ARMSTUB8_ARCH_H_

#define BIT(x) (1 << (x))

#define SCR_RW                  BIT(10)
#define SCR_HCE                 BIT(8)
#define SCR_SMD                 BIT(7)
#define SCR_RES1_5              BIT(5)
#define SCR_RES1_4              BIT(4)
#define SCR_NS                  BIT(0)
#define SCR_VAL (SCR_RW | SCR_HCE | SCR_RES1_5 | SCR_RES1_4 | SCR_NS)


#define SPSR_EL3_D              BIT(9)
#define SPSR_EL3_A              BIT(8)
#define SPSR_EL3_I              BIT(7)
#define SPSR_EL3_F              BIT(6)
#define SPSR_EL3_MODE_EL2H      9
#define SPSR_EL3_VAL (SPSR_EL3_D | SPSR_EL3_A | SPSR_EL3_I | SPSR_EL3_F | SPSR_EL3_MODE_EL2H)

#endif
