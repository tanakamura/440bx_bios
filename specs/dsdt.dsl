/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20251212 (64-bit version)
 * Copyright (c) 2000 - 2025 Intel Corporation
 * 
 * Disassembly of ACPITBL.BIN
 *
 * ACPI Data Table [RSDT]
 *
 * Format: [HexOffset DecimalOffset ByteLength]  FieldName : FieldValue (in hex)
 */

[000h 0000 004h]                   Signature : "RSDT"    [Root System Description Table]
[004h 0004 004h]                Table Length : 0000002C
[008h 0008 001h]                    Revision : 01
[009h 0009 001h]                    Checksum : 00     /* Incorrect checksum, should be FE */
[00Ah 0010 006h]                      Oem ID : "ASUS  "
[010h 0016 008h]                Oem Table ID : "P2B98-XV"
[018h 0024 004h]                Oem Revision : 58582E31
[01Ch 0028 004h]             Asl Compiler ID : "ASUS"
[020h 0032 004h]       Asl Compiler Revision : 31303030

[024h 0036 004h]       ACPI Table Address   0 : 00000000
[028h 0040 004h]       ACPI Table Address   1 : 00000000

Raw Table Data: Length 44 (0x2C)

    0000: 52 53 44 54 2C 00 00 00 01 00 41 53 55 53 20 20  // RSDT,.....ASUS  
    0010: 50 32 42 39 38 2D 58 56 31 2E 58 58 41 53 55 53  // P2B98-XV1.XXASUS
    0020: 30 30 30 31 00 00 00 00 00 00 00 00              // 0001........
/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20251212 (64-bit version)
 * Copyright (c) 2000 - 2025 Intel Corporation
 * 
 * Disassembly of ACPITBL.BIN
 *
 * ACPI Data Table [BOOT]
 *
 * Format: [HexOffset DecimalOffset ByteLength]  FieldName : FieldValue (in hex)
 */

[000h 0000 004h]                   Signature : "BOOT"    [Simple Boot Flag Table]
[004h 0004 004h]                Table Length : 00000028
[008h 0008 001h]                    Revision : 01
[009h 0009 001h]                    Checksum : 00     /* Incorrect checksum, should be C5 */
[00Ah 0010 006h]                      Oem ID : "ASUS  "
[010h 0016 008h]                Oem Table ID : "P2B98-XV"
[018h 0024 004h]                Oem Revision : 58582E31
[01Ch 0028 004h]             Asl Compiler ID : "ASUS"
[020h 0032 004h]       Asl Compiler Revision : 31303030

[024h 0036 001h]         Boot Register Index : 46
[025h 0037 003h]                    Reserved : 000000

Raw Table Data: Length 40 (0x28)

    0000: 42 4F 4F 54 28 00 00 00 01 00 41 53 55 53 20 20  // BOOT(.....ASUS  
    0010: 50 32 42 39 38 2D 58 56 31 2E 58 58 41 53 55 53  // P2B98-XV1.XXASUS
    0020: 30 30 30 31 46 00 00 00                          // 0001F...
/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20251212 (64-bit version)
 * Copyright (c) 2000 - 2025 Intel Corporation
 * 
 * Disassembly of ACPITBL.BIN
 *
 * ACPI Data Table [FACP]
 *
 * Format: [HexOffset DecimalOffset ByteLength]  FieldName : FieldValue (in hex)
 */

[000h 0000 004h]                   Signature : "FACP"    [Fixed ACPI Description Table (FADT)]
[004h 0004 004h]                Table Length : 00000074
[008h 0008 001h]                    Revision : 01
[009h 0009 001h]                    Checksum : 00     /* Incorrect checksum, should be 93 */
[00Ah 0010 006h]                      Oem ID : "ASUS  "
[010h 0016 008h]                Oem Table ID : "P2B98-XV"
[018h 0024 004h]                Oem Revision : 58582E31
[01Ch 0028 004h]             Asl Compiler ID : "ASUS"
[020h 0032 004h]       Asl Compiler Revision : 31303030

[024h 0036 004h]                FACS Address : 00000000
[028h 0040 004h]                DSDT Address : 00000000
[02Ch 0044 001h]                       Model : 00
[02Dh 0045 001h]                  PM Profile : 00 [Unspecified]
[02Eh 0046 002h]               SCI Interrupt : 0009
[030h 0048 004h]            SMI Command Port : 000000B2
[034h 0052 001h]           ACPI Enable Value : A1
[035h 0053 001h]          ACPI Disable Value : A0
[036h 0054 001h]              S4BIOS Command : 00
[037h 0055 001h]             P-State Control : 00
[038h 0056 004h]    PM1A Event Block Address : 0000E400
[03Ch 0060 004h]    PM1B Event Block Address : 00000000
[040h 0064 004h]  PM1A Control Block Address : 0000E404
[044h 0068 004h]  PM1B Control Block Address : 00000000
[048h 0072 004h]   PM2 Control Block Address : 00000000
[04Ch 0076 004h]      PM Timer Block Address : 0000E408
[050h 0080 004h]          GPE0 Block Address : 0000E40C
[054h 0084 004h]          GPE1 Block Address : 00000000
[058h 0088 001h]      PM1 Event Block Length : 04
[059h 0089 001h]    PM1 Control Block Length : 02
[05Ah 0090 001h]    PM2 Control Block Length : 00
[05Bh 0091 001h]       PM Timer Block Length : 04
[05Ch 0092 001h]           GPE0 Block Length : 04
[05Dh 0093 001h]           GPE1 Block Length : 00
[05Eh 0094 001h]            GPE1 Base Offset : 00
[05Fh 0095 001h]                _CST Support : 00
[060h 0096 002h]                  C2 Latency : 005A
[062h 0098 002h]                  C3 Latency : 0384
[064h 0100 002h]              CPU Cache Size : 0000
[066h 0102 002h]          Cache Flush Stride : 0000
[068h 0104 001h]           Duty Cycle Offset : 01
[069h 0105 001h]            Duty Cycle Width : 00
[06Ah 0106 001h]         RTC Day Alarm Index : 0D
[06Bh 0107 001h]       RTC Month Alarm Index : 00
[06Ch 0108 001h]           RTC Century Index : 00
[06Dh 0109 002h]  Boot Flags (decoded below) : 0000
               Legacy Devices Supported (V2) : 0
            8042 Present on ports 60/64 (V2) : 0
                        VGA Not Present (V4) : 0
                      MSI Not Supported (V4) : 0
                PCIe ASPM Not Supported (V4) : 0
                   CMOS RTC Not Present (V5) : 0
[06Fh 0111 001h]                    Reserved : 00
[070h 0112 004h]       Flags (decoded below) : 000000A5
      WBINVD instruction is operational (V1) : 1
              WBINVD flushes all caches (V1) : 0
                    All CPUs support C1 (V1) : 1
                  C2 works on MP system (V1) : 0
            Control Method Power Button (V1) : 0
            Control Method Sleep Button (V1) : 1
        RTC wake not in fixed reg space (V1) : 0
            RTC can wake system from S4 (V1) : 1
                        32-bit PM Timer (V1) : 0
                      Docking Supported (V1) : 0
               Reset Register Supported (V2) : 0
                            Sealed Case (V3) : 0
                    Headless - No Video (V3) : 0
        Use native instr after SLP_TYPx (V3) : 0
              PCIEXP_WAK Bits Supported (V4) : 0
                     Use Platform Timer (V4) : 0
               RTC_STS valid on S4 wake (V4) : 0
                Remote Power-on capable (V4) : 0
                 Use APIC Cluster Model (V4) : 0
     Use APIC Physical Destination Mode (V4) : 0
                       Hardware Reduced (V5) : 0
                      Low Power S0 Idle (V5) : 0

Raw Table Data: Length 116 (0x74)

    0000: 46 41 43 50 74 00 00 00 01 00 41 53 55 53 20 20  // FACPt.....ASUS  
    0010: 50 32 42 39 38 2D 58 56 31 2E 58 58 41 53 55 53  // P2B98-XV1.XXASUS
    0020: 30 30 30 31 00 00 00 00 00 00 00 00 00 00 09 00  // 0001............
    0030: B2 00 00 00 A1 A0 00 00 00 E4 00 00 00 00 00 00  // ................
    0040: 04 E4 00 00 00 00 00 00 00 00 00 00 08 E4 00 00  // ................
    0050: 0C E4 00 00 00 00 00 00 04 02 00 04 04 00 00 00  // ................
    0060: 5A 00 84 03 00 00 00 00 01 00 0D 00 00 00 00 00  // Z...............
    0070: A5 00 00 00                                      // ....
/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20251212 (64-bit version)
 * Copyright (c) 2000 - 2025 Intel Corporation
 * 
 * Disassembly of ACPITBL.BIN
 *
 * ACPI Data Table [FACS]
 *
 * Format: [HexOffset DecimalOffset ByteLength]  FieldName : FieldValue (in hex)
 */

[000h 0000 004h]                   Signature : "FACS"
[004h 0004 004h]                      Length : 00000040
[008h 0008 004h]          Hardware Signature : 00000000
[00Ch 0012 004h]   32 Firmware Waking Vector : 00000000
[010h 0016 004h]                 Global Lock : 00000000
[014h 0020 004h]       Flags (decoded below) : 00000000
                      S4BIOS Support Present : 0
                  64-bit Wake Supported (V2) : 0
[018h 0024 008h]   64 Firmware Waking Vector : 0000000000000000
[020h 0032 001h]                     Version : 00
[021h 0033 003h]                    Reserved : 000000
[024h 0036 004h]   OspmFlags (decoded below) : 00000000
               64-bit Wake Env Required (V2) : 0

Raw Table Data: Length 64 (0x40)

    0000: 46 41 43 53 40 00 00 00 00 00 00 00 00 00 00 00  // FACS@...........
    0010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  // ................
    0020: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  // ................
    0030: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  // ................
/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20251212 (64-bit version)
 * Copyright (c) 2000 - 2025 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of ACPITBL.BIN
 *
 * Original Table Header:
 *     Signature        "DSDT"
 *     Length           0x0000199C (6556)
 *     Revision         0x01 **** 32-bit table (V1), no 64-bit math support
 *     Checksum         0x9A
 *     OEM ID           "ASUS"
 *     OEM Table ID     "P2B98-XV"
 *     OEM Revision     0x00001000 (4096)
 *     Compiler ID      "MSFT"
 *     Compiler Version 0x01000001 (16777217)
 */
DefinitionBlock ("", "DSDT", 1, "ASUS", "P2B98-XV", 0x00001000)
{
    Scope (\_PR)
    {
        Processor (\_PR.CPU0, 0x01, 0x0000E410, 0x06){}
    }

    Name (\_S0, Package (0x04)  // _S0_: S0 System State
    {
        0x05, 
        0x05, 
        0x00, 
        0x00
    })
    Name (\_S1, Package (0x04)  // _S1_: S1 System State
    {
        0x07, 
        0x07, 
        0x00, 
        0x00
    })
    Name (\_S5, Package (0x04)  // _S5_: S5 System State
    {
        0x06, 
        0x06, 
        0x00, 
        0x00
    })
    OperationRegion (\DEBG, SystemIO, 0x80, 0x01)
    Field (\DEBG, ByteAcc, NoLock, Preserve)
    {
        DBG1,   8
    }

    OperationRegion (GPOB, SystemIO, 0xE42C, 0x10)
    Field (GPOB, ByteAcc, NoLock, Preserve)
    {
        Offset (0x03), 
        TO12,   1, 
        Offset (0x08), 
        FANM,   1, 
        Offset (0x09), 
        PLED,   1, 
            ,   3, 
            ,   2, 
            ,   16, 
        MSG0,   1
    }

    Method (\_PTS, 1, NotSerialized)  // _PTS: Prepare To Sleep
    {
        If ((Arg0 != 0x05))
        {
            FANM = 0x00
            PLED = 0x00
        }

        If ((Arg0 == 0x01))
        {
            TO12 = One
        }

        If ((Arg0 == 0x02)){}
        TO12 = One
        Local2 = (Arg0 | 0xF0)
        DBG1 = Local2
    }

    Method (\_WAK, 1, NotSerialized)  // _WAK: Wake
    {
        Notify (\_SB.PWRB, 0x02) // Device Wake
        FANM = 0x01
        PLED = One
        DBG1 = 0xFF
    }

    Scope (\_SI)
    {
        Method (_MSG, 1, NotSerialized)  // _MSG: Message
        {
            If ((Arg0 == Zero))
            {
                MSG0 = One
            }
            Else
            {
                MSG0 = Zero
            }
        }
    }

    Scope (\_SB)
    {
        Device (PWRB)
        {
            Name (_HID, EisaId ("PNP0C0C") /* Power Button Device */)  // _HID: Hardware ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0B)
            }
        }

        OperationRegion (MGRM, SystemMemory, 0x04E3, 0x02)
        Field (MGRM, WordAcc, NoLock, Preserve)
        {
            MEM0,   16
        }

        Method (MEMS, 0, NotSerialized)
        {
            Return (MEM0) /* \_SB_.MEM0 */
        }

        Device (MEM)
        {
            Name (_HID, EisaId ("PNP0C01") /* System Board */)  // _HID: Hardware ID
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Name (BUF1, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite,
                        0x00000000,         // Address Base
                        0x000A0000,         // Address Length
                        )
                    Memory32Fixed (ReadOnly,
                        0x000F0000,         // Address Base
                        0x00010000,         // Address Length
                        )
                    Memory32Fixed (ReadWrite,
                        0x00100000,         // Address Base
                        0x00000000,         // Address Length
                        _Y00)
                    Memory32Fixed (ReadOnly,
                        0xFFFE0000,         // Address Base
                        0x00020000,         // Address Length
                        )
                })
                CreateDWordField (BUF1, \_SB.MEM._CRS._Y00._LEN, EMLN)  // _LEN: Length
                EMLN = MEMS ()
                EMLN <<= 0x14
                Return (BUF1) /* \_SB_.MEM_._CRS.BUF1 */
            }
        }

        Device (LNKA)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x01)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Local0 = (\_SB.PCI0.PX40.PIRA & 0x8F)
                If ((Local0 < 0x80))
                {
                    Return (0x0B)
                }
                Else
                {
                    Return (0x09)
                }
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {3,4,5,6,7,9,10,11,12,14,15}
            })
            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.PX40.PIRA = 0x80
            }

            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Name (BUFA, ResourceTemplate ()
                {
                    IRQ (Level, ActiveLow, Shared, _Y01)
                        {}
                })
                CreateWordField (BUFA, \_SB.LNKA._CRS._Y01._INT, IRA)  // _INT: Interrupts
                Local0 = (\_SB.PCI0.PX40.PIRA & 0x8F)
                If ((Local0 < 0x80))
                {
                    Local0 &= 0x0F
                    Local1 = (One << Local0)
                    IRA = Local1
                }

                Return (BUFA) /* \_SB_.LNKA._CRS.BUFA */
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateByteField (Arg0, 0x01, IRA1)
                CreateByteField (Arg0, 0x02, IRA2)
                Local0 = (IRA2 << 0x08)
                Local0 |= IRA1 /* \_SB_.LNKA._SRS.IRA1 */
                Local1 = 0x00
                Local0 >>= 0x01
                While ((Local0 > 0x00))
                {
                    Local1++
                    Local0 >>= 0x01
                }

                Local0 = (\_SB.PCI0.PX40.PIRA & 0x70)
                \_SB.PCI0.PX40.PIRA = (Local1 | Local0)
            }
        }

        Device (LNKB)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x02)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Local0 = (\_SB.PCI0.PX40.PIRB & 0x8F)
                If ((Local0 < 0x80))
                {
                    Return (0x0B)
                }
                Else
                {
                    Return (0x09)
                }
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {3,4,5,6,7,9,10,11,12,14,15}
            })
            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.PX40.PIRB = 0x80
            }

            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Name (BUFB, ResourceTemplate ()
                {
                    IRQ (Level, ActiveLow, Shared, _Y02)
                        {}
                })
                CreateWordField (BUFB, \_SB.LNKB._CRS._Y02._INT, IRB)  // _INT: Interrupts
                Local0 = (\_SB.PCI0.PX40.PIRB & 0x8F)
                If ((Local0 < 0x80))
                {
                    Local0 &= 0x0F
                    Local1 = (One << Local0)
                    IRB = Local1
                }

                Return (BUFB) /* \_SB_.LNKB._CRS.BUFB */
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateByteField (Arg0, 0x01, IRB1)
                CreateByteField (Arg0, 0x02, IRB2)
                Local0 = (IRB2 << 0x08)
                Local0 |= IRB1 /* \_SB_.LNKB._SRS.IRB1 */
                Local1 = 0x00
                Local0 >>= 0x01
                While ((Local0 > 0x00))
                {
                    Local1++
                    Local0 >>= 0x01
                }

                Local0 = (\_SB.PCI0.PX40.PIRB & 0x70)
                \_SB.PCI0.PX40.PIRB = (Local1 | Local0)
            }
        }

        Device (LNKC)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x03)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Local0 = (\_SB.PCI0.PX40.PIRC & 0x8F)
                If ((Local0 < 0x80))
                {
                    Return (0x0B)
                }
                Else
                {
                    Return (0x09)
                }
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {3,4,5,6,7,9,10,11,12,14,15}
            })
            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.PX40.PIRC = 0x80
            }

            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Name (BUFC, ResourceTemplate ()
                {
                    IRQ (Level, ActiveLow, Shared, _Y03)
                        {}
                })
                CreateWordField (BUFC, \_SB.LNKC._CRS._Y03._INT, IRC)  // _INT: Interrupts
                Local0 = (\_SB.PCI0.PX40.PIRC & 0x8F)
                If ((Local0 < 0x80))
                {
                    Local0 &= 0x0F
                    Local1 = (One << Local0)
                    IRC = Local1
                }

                Return (BUFC) /* \_SB_.LNKC._CRS.BUFC */
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateByteField (Arg0, 0x01, IRC1)
                CreateByteField (Arg0, 0x02, IRC2)
                Local0 = (IRC2 << 0x08)
                Local0 |= IRC1 /* \_SB_.LNKC._SRS.IRC1 */
                Local1 = 0x00
                Local0 >>= 0x01
                While ((Local0 > 0x00))
                {
                    Local1++
                    Local0 >>= 0x01
                }

                Local0 = (\_SB.PCI0.PX40.PIRC & 0x70)
                \_SB.PCI0.PX40.PIRC = (Local1 | Local0)
            }
        }

        Device (LNKD)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x04)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Local0 = (\_SB.PCI0.PX40.PIRD & 0x8F)
                If ((Local0 < 0x80))
                {
                    Return (0x0B)
                }
                Else
                {
                    Return (0x09)
                }
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {3,4,5,6,7,9,10,11,12,14,15}
            })
            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.PX40.PIRD = 0x80
            }

            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Name (BUFD, ResourceTemplate ()
                {
                    IRQ (Level, ActiveLow, Shared, _Y04)
                        {}
                })
                CreateWordField (BUFD, \_SB.LNKD._CRS._Y04._INT, IRD)  // _INT: Interrupts
                Local0 = (\_SB.PCI0.PX40.PIRD & 0x8F)
                If ((Local0 < 0x80))
                {
                    Local0 &= 0x0F
                    Local1 = (One << Local0)
                    IRD = Local1
                }

                Return (BUFD) /* \_SB_.LNKD._CRS.BUFD */
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateByteField (Arg0, 0x01, IRD1)
                CreateByteField (Arg0, 0x02, IRD2)
                Local0 = (IRD2 << 0x08)
                Local0 |= IRD1 /* \_SB_.LNKD._SRS.IRD1 */
                Local1 = 0x00
                Local0 >>= 0x01
                While ((Local0 > 0x00))
                {
                    Local1++
                    Local0 >>= 0x01
                }

                Local0 = (\_SB.PCI0.PX40.PIRD & 0x70)
                \_SB.PCI0.PX40.PIRD = (Local1 | Local0)
            }
        }

        Device (PCI0)
        {
            Name (_HID, EisaId ("PNP0A03") /* PCI Bus */)  // _HID: Hardware ID
            Name (_ADR, 0x00)  // _ADR: Address
            Name (CRES, ResourceTemplate ()
            {
                WordBusNumber (ResourceProducer, MinFixed, MaxFixed, PosDecode,
                    0x0000,             // Granularity
                    0x0000,             // Range Minimum
                    0x00FF,             // Range Maximum
                    0x0000,             // Translation Offset
                    0x0100,             // Length
                    ,, )
                IO (Decode16,
                    0x0CF8,             // Range Minimum
                    0x0CF8,             // Range Maximum
                    0x01,               // Alignment
                    0x08,               // Length
                    )
                WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x0000,             // Granularity
                    0x0000,             // Range Minimum
                    0x0CF7,             // Range Maximum
                    0x0000,             // Translation Offset
                    0x0CF8,             // Length
                    ,, , TypeStatic, DenseTranslation)
                WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x0000,             // Granularity
                    0x0D00,             // Range Minimum
                    0xFFFF,             // Range Maximum
                    0x0000,             // Translation Offset
                    0xF300,             // Length
                    ,, , TypeStatic, DenseTranslation)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000A0000,         // Range Minimum
                    0x000BFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00020000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000C8000,         // Range Minimum
                    0x000DFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00018000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x00100000,         // Range Minimum
                    0xFFFFFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0xFFF00000,         // Length
                    ,, _Y05, AddressRangeMemory, TypeStatic)
            })
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                CreateDWordField (CRES, \_SB.PCI0._Y05._MIN, RAMT)  // _MIN: Minimum Base Address
                CreateDWordField (CRES, \_SB.PCI0._Y05._LEN, RAMR)  // _LEN: Length
                RAMT = (MEMS () + 0x01)
                RAMT <<= 0x14
                RAMR = (0xFFFE0000 - RAMT) /* \_SB_.PCI0._CRS.RAMT */
                Return (CRES) /* \_SB_.PCI0.CRES */
            }

            Name (_PRT, Package (0x18)  // _PRT: PCI Routing Table
            {
                Package (0x04)
                {
                    0x000CFFFF, 
                    0x00, 
                    \_SB.LNKA, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x000CFFFF, 
                    0x01, 
                    \_SB.LNKB, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x000CFFFF, 
                    0x02, 
                    \_SB.LNKC, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x000CFFFF, 
                    0x03, 
                    \_SB.LNKD, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x000BFFFF, 
                    0x00, 
                    \_SB.LNKB, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x000BFFFF, 
                    0x01, 
                    \_SB.LNKC, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x000BFFFF, 
                    0x02, 
                    \_SB.LNKD, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x000BFFFF, 
                    0x03, 
                    \_SB.LNKA, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x000AFFFF, 
                    0x00, 
                    \_SB.LNKC, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x000AFFFF, 
                    0x01, 
                    \_SB.LNKD, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x000AFFFF, 
                    0x02, 
                    \_SB.LNKA, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x000AFFFF, 
                    0x03, 
                    \_SB.LNKB, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0009FFFF, 
                    0x00, 
                    \_SB.LNKD, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0009FFFF, 
                    0x01, 
                    \_SB.LNKA, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0009FFFF, 
                    0x02, 
                    \_SB.LNKB, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0009FFFF, 
                    0x03, 
                    \_SB.LNKC, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0007FFFF, 
                    0x00, 
                    \_SB.LNKA, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0007FFFF, 
                    0x01, 
                    \_SB.LNKB, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0007FFFF, 
                    0x02, 
                    \_SB.LNKC, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0007FFFF, 
                    0x03, 
                    \_SB.LNKD, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    0x00, 
                    \_SB.LNKA, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    0x01, 
                    \_SB.LNKB, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    0x02, 
                    \_SB.LNKC, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    0x03, 
                    \_SB.LNKD, 
                    0x00
                }
            })
            Device (PX40)
            {
                Name (_ADR, 0x00070000)  // _ADR: Address
                OperationRegion (PIRQ, PCI_Config, 0x60, 0x04)
                Field (PIRQ, ByteAcc, NoLock, Preserve)
                {
                    PIRA,   8, 
                    PIRB,   8, 
                    PIRC,   8, 
                    PIRD,   8
                }

                Device (SYSR)
                {
                    Name (_HID, EisaId ("PNP0C02") /* PNP Motherboard Resources */)  // _HID: Hardware ID
                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Name (BUF1, ResourceTemplate ()
                        {
                            IO (Decode16,
                                0x0000,             // Range Minimum
                                0x0000,             // Range Maximum
                                0x01,               // Alignment
                                0x40,               // Length
                                _Y06)
                            IO (Decode16,
                                0x0000,             // Range Minimum
                                0x0000,             // Range Maximum
                                0x01,               // Alignment
                                0x10,               // Length
                                _Y07)
                            IO (Decode16,
                                0x0010,             // Range Minimum
                                0x0010,             // Range Maximum
                                0x01,               // Alignment
                                0x10,               // Length
                                )
                            IO (Decode16,
                                0x0022,             // Range Minimum
                                0x0022,             // Range Maximum
                                0x01,               // Alignment
                                0x1E,               // Length
                                )
                            IO (Decode16,
                                0x0044,             // Range Minimum
                                0x0044,             // Range Maximum
                                0x01,               // Alignment
                                0x1C,               // Length
                                )
                            IO (Decode16,
                                0x0062,             // Range Minimum
                                0x0062,             // Range Maximum
                                0x01,               // Alignment
                                0x02,               // Length
                                )
                            IO (Decode16,
                                0x0065,             // Range Minimum
                                0x0065,             // Range Maximum
                                0x01,               // Alignment
                                0x0B,               // Length
                                )
                            IO (Decode16,
                                0x0074,             // Range Minimum
                                0x0074,             // Range Maximum
                                0x01,               // Alignment
                                0x0C,               // Length
                                )
                            IO (Decode16,
                                0x0091,             // Range Minimum
                                0x0091,             // Range Maximum
                                0x01,               // Alignment
                                0x03,               // Length
                                )
                            IO (Decode16,
                                0x00A2,             // Range Minimum
                                0x00A2,             // Range Maximum
                                0x01,               // Alignment
                                0x1E,               // Length
                                )
                            IO (Decode16,
                                0x00E0,             // Range Minimum
                                0x00E0,             // Range Maximum
                                0x01,               // Alignment
                                0x10,               // Length
                                )
                            IO (Decode16,
                                0x015C,             // Range Minimum
                                0x015C,             // Range Maximum
                                0x01,               // Alignment
                                0x02,               // Length
                                )
                            IO (Decode16,
                                0x03F0,             // Range Minimum
                                0x03F0,             // Range Maximum
                                0x01,               // Alignment
                                0x02,               // Length
                                )
                            IO (Decode16,
                                0x04D0,             // Range Minimum
                                0x04D0,             // Range Maximum
                                0x01,               // Alignment
                                0x02,               // Length
                                )
                        })
                        CreateByteField (BUF1, \_SB.PCI0.PX40.SYSR._CRS._Y06._MIN, PMLO)  // _MIN: Minimum Base Address
                        CreateByteField (BUF1, 0x03, PMHI)
                        CreateByteField (BUF1, \_SB.PCI0.PX40.SYSR._CRS._Y06._MAX, PMRL)  // _MAX: Maximum Base Address
                        CreateByteField (BUF1, 0x05, PMRH)
                        CreateByteField (BUF1, \_SB.PCI0.PX40.SYSR._CRS._Y07._MIN, SBLO)  // _MIN: Minimum Base Address
                        CreateByteField (BUF1, 0x0B, SBHI)
                        CreateByteField (BUF1, \_SB.PCI0.PX40.SYSR._CRS._Y07._MAX, SBRL)  // _MAX: Maximum Base Address
                        CreateByteField (BUF1, 0x0D, SBRH)
                        Local0 = \_SB.PCI0.PX43.PM00
                        PMLO = (Local0 & 0xFE)
                        PMHI = \_SB.PCI0.PX43.PM01
                        Local0 = \_SB.PCI0.PX43.SB00
                        SBLO = (Local0 & 0xFE)
                        SBHI = \_SB.PCI0.PX43.SB01
                        PMRL = PMLO /* \_SB_.PCI0.PX40.SYSR._CRS.PMLO */
                        PMRH = PMHI /* \_SB_.PCI0.PX40.SYSR._CRS.PMHI */
                        SBRL = SBLO /* \_SB_.PCI0.PX40.SYSR._CRS.SBLO */
                        SBRH = SBHI /* \_SB_.PCI0.PX40.SYSR._CRS.SBHI */
                        Return (BUF1) /* \_SB_.PCI0.PX40.SYSR._CRS.BUF1 */
                    }
                }

                Device (PIC)
                {
                    Name (_HID, EisaId ("PNP0000") /* 8259-compatible Programmable Interrupt Controller */)  // _HID: Hardware ID
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0020,             // Range Minimum
                            0x0020,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00A0,             // Range Minimum
                            0x00A0,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IRQNoFlags ()
                            {2}
                    })
                }

                Device (DMA1)
                {
                    Name (_HID, EisaId ("PNP0200") /* PC-class DMA Controller */)  // _HID: Hardware ID
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        DMA (Compatibility, BusMaster, Transfer8, )
                            {4}
                        IO (Decode16,
                            0x0000,             // Range Minimum
                            0x0000,             // Range Maximum
                            0x01,               // Alignment
                            0x10,               // Length
                            )
                        IO (Decode16,
                            0x0080,             // Range Minimum
                            0x0080,             // Range Maximum
                            0x01,               // Alignment
                            0x11,               // Length
                            )
                        IO (Decode16,
                            0x0094,             // Range Minimum
                            0x0094,             // Range Maximum
                            0x01,               // Alignment
                            0x0C,               // Length
                            )
                        IO (Decode16,
                            0x00C0,             // Range Minimum
                            0x00C0,             // Range Maximum
                            0x01,               // Alignment
                            0x20,               // Length
                            )
                    })
                }

                Device (TMR)
                {
                    Name (_HID, EisaId ("PNP0100") /* PC-class System Timer */)  // _HID: Hardware ID
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0040,             // Range Minimum
                            0x0040,             // Range Maximum
                            0x01,               // Alignment
                            0x04,               // Length
                            )
                        IRQNoFlags ()
                            {0}
                    })
                }

                Device (RTC)
                {
                    Name (_HID, EisaId ("PNP0B00") /* AT Real-Time Clock */)  // _HID: Hardware ID
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0070,             // Range Minimum
                            0x0070,             // Range Maximum
                            0x01,               // Alignment
                            0x04,               // Length
                            )
                        IRQNoFlags ()
                            {8}
                    })
                }

                Device (SPKR)
                {
                    Name (_HID, EisaId ("PNP0800") /* Microsoft Sound System Compatible Device */)  // _HID: Hardware ID
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0061,             // Range Minimum
                            0x0061,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                    })
                }

                Device (COPR)
                {
                    Name (_HID, EisaId ("PNP0C04") /* x87-compatible Floating Point Processing Unit */)  // _HID: Hardware ID
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x00F0,             // Range Minimum
                            0x00F0,             // Range Maximum
                            0x01,               // Alignment
                            0x10,               // Length
                            )
                        IRQNoFlags ()
                            {13}
                    })
                }

                OperationRegion (N309, SystemIO, 0x015C, 0x02)
                Field (N309, ByteAcc, NoLock, Preserve)
                {
                    NIDX,   8, 
                    NDAT,   8
                }

                IndexField (NIDX, NDAT, ByteAcc, NoLock, Preserve)
                {
                    Offset (0x07), 
                    LDNM,   8, 
                    Offset (0x30), 
                    ACTR,   8, 
                    Offset (0x60), 
                    IOAH,   8, 
                    IOAL,   8, 
                    IOBH,   8, 
                    IOBL,   8, 
                    Offset (0x70), 
                    INTR,   8, 
                    INTP,   8, 
                    Offset (0x74), 
                    DMCH,   8, 
                    Offset (0xF0), 
                    OPT1,   8, 
                    OPT2,   8
                }

                Method (CKIR, 0, NotSerialized)
                {
                    LDNM = 0x02
                    Local5 = ACTR /* \_SB_.PCI0.PX40.ACTR */
                    ACTR = One
                    OPT1 |= 0x80
                    Local0 = IOAH /* \_SB_.PCI0.PX40.IOAH */
                    Local0 <<= 0x08
                    Local0 |= IOAL /* \_SB_.PCI0.PX40.IOAL */
                    Local0 += 0x02
                    OperationRegion (OIRC, SystemIO, Local0, 0x02)
                    Field (OIRC, ByteAcc, NoLock, Preserve)
                    {
                        IRCR,   8, 
                        IBSR,   8
                    }

                    IBSR = 0xE8
                    Local1 = IRCR /* \_SB_.PCI0.PX40.CKIR.IRCR */
                    IBSR = 0x00
                    OPT1 &= 0x7F
                    DBG1 = Local1
                    Local1 &= 0x0C
                    If ((Local1 == 0x00))
                    {
                        Local1 = 0x00
                    }
                    Else
                    {
                        Local1 = 0x01
                    }

                    ACTR = Local5
                    Return (Local1)
                }

                Device (FDC0)
                {
                    Name (_HID, EisaId ("PNP0700"))  // _HID: Hardware ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        LDNM = Zero
                        If (ACTR)
                        {
                            Return (0x0F)
                        }
                        Else
                        {
                            Return (0x05)
                        }
                    }

                    Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
                    {
                        LDNM = Zero
                        ACTR = Zero
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Name (BUF0, ResourceTemplate ()
                        {
                            IO (Decode16,
                                0x03F2,             // Range Minimum
                                0x03F2,             // Range Maximum
                                0x00,               // Alignment
                                0x04,               // Length
                                )
                            IO (Decode16,
                                0x03F7,             // Range Minimum
                                0x03F7,             // Range Maximum
                                0x00,               // Alignment
                                0x01,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, _Y08)
                                {6}
                            DMA (Compatibility, NotBusMaster, Transfer8, _Y09)
                                {2}
                        })
                        CreateWordField (BUF0, \_SB.PCI0.PX40.FDC0._CRS._Y08._INT, IRQW)  // _INT: Interrupts
                        CreateByteField (BUF0, \_SB.PCI0.PX40.FDC0._CRS._Y09._DMA, DMAV)  // _DMA: Direct Memory Access
                        LDNM = Zero
                        Local0 = One
                        IRQW = (Local0 << INTR) /* \_SB_.PCI0.PX40.INTR */
                        Local0 = One
                        DMAV = (Local0 << DMCH) /* \_SB_.PCI0.PX40.DMCH */
                        Return (BUF0) /* \_SB_.PCI0.PX40.FDC0._CRS.BUF0 */
                    }

                    Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
                    {
                        IO (Decode16,
                            0x03F2,             // Range Minimum
                            0x03F2,             // Range Maximum
                            0x00,               // Alignment
                            0x04,               // Length
                            )
                        IO (Decode16,
                            0x03F7,             // Range Minimum
                            0x03F7,             // Range Maximum
                            0x00,               // Alignment
                            0x01,               // Length
                            )
                        IRQ (Edge, ActiveHigh, Exclusive, )
                            {6}
                        DMA (Compatibility, NotBusMaster, Transfer8, )
                            {2}
                    })
                    Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
                    {
                        CreateByteField (Arg0, 0x02, IOLO)
                        CreateByteField (Arg0, 0x03, IOHI)
                        CreateWordField (Arg0, 0x11, IRQW)
                        CreateByteField (Arg0, 0x15, DMAV)
                        LDNM = Zero
                        IOAH = IOHI /* \_SB_.PCI0.PX40.FDC0._SRS.IOHI */
                        IOAL = IOLO /* \_SB_.PCI0.PX40.FDC0._SRS.IOLO */
                        FindSetRightBit (IRQW, Local0)
                        Local0--
                        INTR = Local0
                        FindSetRightBit (DMAV, Local0)
                        Local0--
                        DMCH = Local0
                        ACTR = One
                    }
                }

                Device (LPT)
                {
                    Name (_HID, EisaId ("PNP0400") /* Standard LPT Parallel Port */)  // _HID: Hardware ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        LDNM = 0x01
                        Local0 = (OPT1 & 0x80)
                        If ((IOAH || IOAL))
                        {
                            If ((Local0 == 0x80))
                            {
                                Return (0x00)
                            }
                            ElseIf (ACTR)
                            {
                                Return (0x0F)
                            }
                            Else
                            {
                                Return (0x05)
                            }
                        }

                        Return (0x00)
                    }

                    Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
                    {
                        LDNM = 0x01
                        ACTR = Zero
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Name (BUF5, ResourceTemplate ()
                        {
                            IO (Decode16,
                                0x0378,             // Range Minimum
                                0x0378,             // Range Maximum
                                0x00,               // Alignment
                                0x04,               // Length
                                _Y0A)
                            IRQ (Edge, ActiveHigh, Exclusive, _Y0B)
                                {7}
                        })
                        CreateByteField (BUF5, \_SB.PCI0.PX40.LPT._CRS._Y0A._MIN, IOLO)  // _MIN: Minimum Base Address
                        CreateByteField (BUF5, 0x03, IOHI)
                        CreateByteField (BUF5, \_SB.PCI0.PX40.LPT._CRS._Y0A._MAX, IORL)  // _MAX: Maximum Base Address
                        CreateByteField (BUF5, 0x05, IORH)
                        CreateWordField (BUF5, \_SB.PCI0.PX40.LPT._CRS._Y0B._INT, IRQW)  // _INT: Interrupts
                        LDNM = 0x01
                        IOLO = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IORL = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IOHI = IOAH /* \_SB_.PCI0.PX40.IOAH */
                        IORH = IOAH /* \_SB_.PCI0.PX40.IOAH */
                        Local0 = One
                        IRQW = (Local0 << INTR) /* \_SB_.PCI0.PX40.INTR */
                        Return (BUF5) /* \_SB_.PCI0.PX40.LPT_._CRS.BUF5 */
                    }

                    Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
                    {
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x0378,             // Range Minimum
                                0x0378,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {5,7}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x0278,             // Range Minimum
                                0x0278,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {5,7}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x03BC,             // Range Minimum
                                0x03BC,             // Range Maximum
                                0x00,               // Alignment
                                0x04,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {5,7}
                        }
                        EndDependentFn ()
                    })
                    Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
                    {
                        CreateByteField (Arg0, 0x02, IOLO)
                        CreateByteField (Arg0, 0x03, IOHI)
                        CreateWordField (Arg0, 0x09, IRQW)
                        LDNM = 0x01
                        IOAL = IOLO /* \_SB_.PCI0.PX40.LPT_._SRS.IOLO */
                        IOAH = IOHI /* \_SB_.PCI0.PX40.LPT_._SRS.IOHI */
                        FindSetRightBit (IRQW, Local0)
                        Local0--
                        INTR = Local0
                        INTP = 0x02
                        ACTR = One
                    }
                }

                Device (ECP)
                {
                    Name (_HID, EisaId ("PNP0401") /* ECP Parallel Port */)  // _HID: Hardware ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        LDNM = 0x01
                        Local0 = (OPT1 & 0x80)
                        If ((IOAH || IOAL))
                        {
                            If ((Local0 == 0x80))
                            {
                                If (ACTR)
                                {
                                    Return (0x0F)
                                }
                                Else
                                {
                                    Return (0x05)
                                }
                            }
                            Else
                            {
                                Return (0x00)
                            }
                        }

                        Return (0x00)
                    }

                    Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
                    {
                        LDNM = 0x01
                        ACTR = Zero
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Name (BUF6, ResourceTemplate ()
                        {
                            IO (Decode16,
                                0x0378,             // Range Minimum
                                0x0378,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                _Y0C)
                            IO (Decode16,
                                0x0778,             // Range Minimum
                                0x0778,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                _Y0D)
                            IRQ (Edge, ActiveHigh, Exclusive, _Y0E)
                                {7}
                            DMA (Compatibility, NotBusMaster, Transfer8, _Y0F)
                                {1}
                        })
                        CreateByteField (BUF6, \_SB.PCI0.PX40.ECP._CRS._Y0C._MIN, IOLO)  // _MIN: Minimum Base Address
                        CreateByteField (BUF6, 0x03, IOHI)
                        CreateByteField (BUF6, \_SB.PCI0.PX40.ECP._CRS._Y0C._MAX, IORL)  // _MAX: Maximum Base Address
                        CreateByteField (BUF6, 0x05, IORH)
                        CreateByteField (BUF6, \_SB.PCI0.PX40.ECP._CRS._Y0D._MIN, IOPL)  // _MIN: Minimum Base Address
                        CreateByteField (BUF6, 0x0B, IOPH)
                        CreateByteField (BUF6, \_SB.PCI0.PX40.ECP._CRS._Y0D._MAX, IOTL)  // _MAX: Maximum Base Address
                        CreateByteField (BUF6, 0x0D, IOTH)
                        CreateWordField (BUF6, \_SB.PCI0.PX40.ECP._CRS._Y0E._INT, IRQW)  // _INT: Interrupts
                        CreateByteField (BUF6, \_SB.PCI0.PX40.ECP._CRS._Y0F._DMA, DMAC)  // _DMA: Direct Memory Access
                        LDNM = 0x01
                        IOLO = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IORL = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IORH = IOAH /* \_SB_.PCI0.PX40.IOAH */
                        IOHI = IOAH /* \_SB_.PCI0.PX40.IOAH */
                        IOPL = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IOTL = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IOPH = (IOAH + 0x04)
                        IOTH = (IOAH + 0x04)
                        Local0 = One
                        IRQW = (Local0 << INTR) /* \_SB_.PCI0.PX40.INTR */
                        Local0 = One
                        DMAC = (Local0 << DMCH) /* \_SB_.PCI0.PX40.DMCH */
                        Return (BUF6) /* \_SB_.PCI0.PX40.ECP_._CRS.BUF6 */
                    }

                    Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
                    {
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x0378,             // Range Minimum
                                0x0378,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IO (Decode16,
                                0x0778,             // Range Minimum
                                0x0778,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {5,7}
                            DMA (Compatibility, NotBusMaster, Transfer8, )
                                {0,1,3}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x0278,             // Range Minimum
                                0x0278,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IO (Decode16,
                                0x0678,             // Range Minimum
                                0x0678,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {5,7}
                            DMA (Compatibility, NotBusMaster, Transfer8, )
                                {0,1,3}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x03BC,             // Range Minimum
                                0x03BC,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IO (Decode16,
                                0x07BC,             // Range Minimum
                                0x07BC,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {5,7}
                            DMA (Compatibility, NotBusMaster, Transfer8, )
                                {0,1,3}
                        }
                        EndDependentFn ()
                    })
                    Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
                    {
                        CreateByteField (Arg0, 0x02, IOLO)
                        CreateByteField (Arg0, 0x03, IOHI)
                        CreateWordField (Arg0, 0x11, IRQW)
                        CreateByteField (Arg0, 0x15, DMAC)
                        LDNM = 0x01
                        IOAL = IOLO /* \_SB_.PCI0.PX40.ECP_._SRS.IOLO */
                        IOAH = IOHI /* \_SB_.PCI0.PX40.ECP_._SRS.IOHI */
                        FindSetRightBit (IRQW, Local0)
                        Local0--
                        INTR = Local0
                        FindSetRightBit (DMAC, Local0)
                        Local0--
                        DMCH = Local0
                        ACTR = One
                    }
                }

                Device (UAR1)
                {
                    Name (_HID, EisaId ("PNP0501") /* 16550A-compatible COM Serial Port */)  // _HID: Hardware ID
                    Name (_UID, 0x01)  // _UID: Unique ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        LDNM = 0x03
                        If ((IOAH || IOAL))
                        {
                            If (ACTR)
                            {
                                Return (0x0F)
                            }
                            Else
                            {
                                Return (0x0D)
                            }
                        }

                        Return (0x00)
                    }

                    Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
                    {
                        LDNM = 0x03
                        ACTR = Zero
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Name (BUF1, ResourceTemplate ()
                        {
                            IO (Decode16,
                                0x03F8,             // Range Minimum
                                0x03F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                _Y10)
                            IRQ (Edge, ActiveHigh, Exclusive, _Y11)
                                {4}
                        })
                        CreateByteField (BUF1, \_SB.PCI0.PX40.UAR1._CRS._Y10._MIN, IOLO)  // _MIN: Minimum Base Address
                        CreateByteField (BUF1, 0x03, IOHI)
                        CreateByteField (BUF1, \_SB.PCI0.PX40.UAR1._CRS._Y10._MAX, IORL)  // _MAX: Maximum Base Address
                        CreateByteField (BUF1, 0x05, IORH)
                        CreateWordField (BUF1, \_SB.PCI0.PX40.UAR1._CRS._Y11._INT, IRQW)  // _INT: Interrupts
                        LDNM = 0x03
                        IOLO = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IORL = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IOHI = IOAH /* \_SB_.PCI0.PX40.IOAH */
                        IORH = IOAH /* \_SB_.PCI0.PX40.IOAH */
                        Local0 = One
                        IRQW = (Local0 << INTR) /* \_SB_.PCI0.PX40.INTR */
                        Return (BUF1) /* \_SB_.PCI0.PX40.UAR1._CRS.BUF1 */
                    }

                    Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
                    {
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x03F8,             // Range Minimum
                                0x03F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {4}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x02F8,             // Range Minimum
                                0x02F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {3}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x03E8,             // Range Minimum
                                0x03E8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {4}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x02E8,             // Range Minimum
                                0x02E8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {5}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x02E8,             // Range Minimum
                                0x03F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {3,4,5,7,12}
                        }
                        EndDependentFn ()
                    })
                    Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
                    {
                        CreateByteField (Arg0, 0x02, IOLO)
                        CreateByteField (Arg0, 0x03, IOHI)
                        CreateWordField (Arg0, 0x09, IRQW)
                        LDNM = 0x03
                        IOAL = IOLO /* \_SB_.PCI0.PX40.UAR1._SRS.IOLO */
                        IOAH = IOHI /* \_SB_.PCI0.PX40.UAR1._SRS.IOHI */
                        FindSetRightBit (IRQW, Local0)
                        Local0--
                        INTR = Local0
                        ACTR = One
                    }

                    Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
                    {
                        0x0A, 
                        0x01
                    })
                }

                Device (IRDA)
                {
                    Name (_HID, EisaId ("PNP0510") /* Generic IRDA-compatible Device */)  // _HID: Hardware ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        LDNM = 0x02
                        If (CKIR ())
                        {
                            If ((IOAH || IOAL))
                            {
                                If (ACTR)
                                {
                                    Return (0x0F)
                                }
                                Else
                                {
                                    Return (0x0D)
                                }
                            }
                        }

                        Return (0x00)
                    }

                    Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
                    {
                        LDNM = 0x02
                        ACTR = Zero
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Name (BUF1, ResourceTemplate ()
                        {
                            IO (Decode16,
                                0x02F8,             // Range Minimum
                                0x02F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                _Y12)
                            IRQ (Edge, ActiveHigh, Exclusive, _Y13)
                                {3}
                        })
                        CreateByteField (BUF1, \_SB.PCI0.PX40.IRDA._CRS._Y12._MIN, IOLO)  // _MIN: Minimum Base Address
                        CreateByteField (BUF1, 0x03, IOHI)
                        CreateByteField (BUF1, \_SB.PCI0.PX40.IRDA._CRS._Y12._MAX, IORL)  // _MAX: Maximum Base Address
                        CreateByteField (BUF1, 0x05, IORH)
                        CreateWordField (BUF1, \_SB.PCI0.PX40.IRDA._CRS._Y13._INT, IRQW)  // _INT: Interrupts
                        LDNM = 0x02
                        IOLO = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IORL = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IOHI = IOAH /* \_SB_.PCI0.PX40.IOAH */
                        IORH = IOAH /* \_SB_.PCI0.PX40.IOAH */
                        Local0 = One
                        IRQW = (Local0 << INTR) /* \_SB_.PCI0.PX40.INTR */
                        Return (BUF1) /* \_SB_.PCI0.PX40.IRDA._CRS.BUF1 */
                    }

                    Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
                    {
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x02F8,             // Range Minimum
                                0x02F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {3}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x03F8,             // Range Minimum
                                0x03F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {4}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x02E8,             // Range Minimum
                                0x02E8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {3}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x03E8,             // Range Minimum
                                0x03E8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {4}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x02E8,             // Range Minimum
                                0x03F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {3,4,5,7,12}
                        }
                        EndDependentFn ()
                    })
                    Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
                    {
                        CreateByteField (Arg0, 0x02, IOLO)
                        CreateByteField (Arg0, 0x03, IOHI)
                        CreateWordField (Arg0, 0x09, IRQW)
                        LDNM = 0x02
                        IOAL = IOLO /* \_SB_.PCI0.PX40.IRDA._SRS.IOLO */
                        IOAH = IOHI /* \_SB_.PCI0.PX40.IRDA._SRS.IOHI */
                        FindSetRightBit (IRQW, Local0)
                        Local0--
                        INTR = Local0
                        ACTR = One
                    }
                }

                Device (UAR2)
                {
                    Name (_HID, EisaId ("PNP0501") /* 16550A-compatible COM Serial Port */)  // _HID: Hardware ID
                    Name (_UID, 0x02)  // _UID: Unique ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        LDNM = 0x02
                        If (CKIR ())
                        {
                            Return (Zero)
                        }
                        Else
                        {
                            If ((IOAH || IOAL))
                            {
                                If (ACTR)
                                {
                                    Return (0x0F)
                                }
                                Else
                                {
                                    Return (0x05)
                                }
                            }

                            Return (0x00)
                        }
                    }

                    Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
                    {
                        LDNM = 0x02
                        ACTR = Zero
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Name (BUF1, ResourceTemplate ()
                        {
                            IO (Decode16,
                                0x02F8,             // Range Minimum
                                0x02F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                _Y14)
                            IRQ (Edge, ActiveHigh, Exclusive, _Y15)
                                {3}
                        })
                        CreateByteField (BUF1, \_SB.PCI0.PX40.UAR2._CRS._Y14._MIN, IOLO)  // _MIN: Minimum Base Address
                        CreateByteField (BUF1, 0x03, IOHI)
                        CreateByteField (BUF1, \_SB.PCI0.PX40.UAR2._CRS._Y14._MAX, IORL)  // _MAX: Maximum Base Address
                        CreateByteField (BUF1, 0x05, IORH)
                        CreateWordField (BUF1, \_SB.PCI0.PX40.UAR2._CRS._Y15._INT, IRQW)  // _INT: Interrupts
                        LDNM = 0x02
                        IOLO = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IORL = IOAL /* \_SB_.PCI0.PX40.IOAL */
                        IOHI = IOAH /* \_SB_.PCI0.PX40.IOAH */
                        IORH = IOAH /* \_SB_.PCI0.PX40.IOAH */
                        Local0 = One
                        IRQW = (Local0 << INTR) /* \_SB_.PCI0.PX40.INTR */
                        Return (BUF1) /* \_SB_.PCI0.PX40.UAR2._CRS.BUF1 */
                    }

                    Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
                    {
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x02F8,             // Range Minimum
                                0x02F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {3}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x03F8,             // Range Minimum
                                0x03F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {4}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x02E8,             // Range Minimum
                                0x02E8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {3}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x03E8,             // Range Minimum
                                0x03E8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {4}
                        }
                        StartDependentFn (0x01, 0x01)
                        {
                            IO (Decode16,
                                0x02E8,             // Range Minimum
                                0x03F8,             // Range Maximum
                                0x00,               // Alignment
                                0x08,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {3,4,5,7,12}
                        }
                        EndDependentFn ()
                    })
                    Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
                    {
                        CreateByteField (Arg0, 0x02, IOLO)
                        CreateByteField (Arg0, 0x03, IOHI)
                        CreateWordField (Arg0, 0x09, IRQW)
                        LDNM = 0x02
                        IOAL = IOLO /* \_SB_.PCI0.PX40.UAR2._SRS.IOLO */
                        IOAH = IOHI /* \_SB_.PCI0.PX40.UAR2._SRS.IOHI */
                        FindSetRightBit (IRQW, Local0)
                        Local0--
                        INTR = Local0
                        ACTR = One
                    }
                }

                OperationRegion (KBC1, SystemIO, 0x64, 0x01)
                Field (KBC1, ByteAcc, NoLock, Preserve)
                {
                    KBIN,   8
                }

                OperationRegion (KBC2, SystemIO, 0x60, 0x01)
                Field (KBC2, ByteAcc, NoLock, Preserve)
                {
                    KBDA,   8
                }

                Device (PS2K)
                {
                    Name (_HID, EisaId ("PNP0303") /* IBM Enhanced Keyboard (101/102-key, PS/2 Mouse) */)  // _HID: Hardware ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        LDNM = 0x06
                        If (ACTR)
                        {
                            Return (0x0F)
                        }
                        Else
                        {
                            Return (0x05)
                        }
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Name (BUF0, ResourceTemplate ()
                        {
                            IO (Decode16,
                                0x0060,             // Range Minimum
                                0x0060,             // Range Maximum
                                0x00,               // Alignment
                                0x01,               // Length
                                )
                            IO (Decode16,
                                0x0064,             // Range Minimum
                                0x0064,             // Range Maximum
                                0x00,               // Alignment
                                0x01,               // Length
                                )
                            IRQ (Edge, ActiveHigh, Exclusive, )
                                {1}
                        })
                        Return (BUF0) /* \_SB_.PCI0.PX40.PS2K._CRS.BUF0 */
                    }
                }

                Device (PS2M)
                {
                    Name (_HID, EisaId ("PNP0F13") /* PS/2 Mouse */)  // _HID: Hardware ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        LDNM = 0x05
                        Local0 = (INTR & 0x0F)
                        If ((Local0 == 0x00))
                        {
                            Return (0x00)
                        }
                        ElseIf (ACTR)
                        {
                            Return (0x0F)
                        }
                        Else
                        {
                            Return (0x05)
                        }
                    }

                    Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
                    {
                        Local0 = (0x03 & KBIN) /* \_SB_.PCI0.PX40.KBIN */
                        While (Local0)
                        {
                            Local1 = KBDA /* \_SB_.PCI0.PX40.KBDA */
                            Local0 = (0x03 & KBIN) /* \_SB_.PCI0.PX40.KBIN */
                        }

                        KBIN = 0xA7
                        LDNM = 0x05
                        ACTR = Zero
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Name (BUF0, ResourceTemplate ()
                        {
                            IRQ (Edge, ActiveHigh, Exclusive, _Y16)
                                {12}
                        })
                        CreateWordField (BUF0, \_SB.PCI0.PX40.PS2M._CRS._Y16._INT, IRQW)  // _INT: Interrupts
                        LDNM = 0x05
                        Local0 = One
                        IRQW = (Local0 << INTR) /* \_SB_.PCI0.PX40.INTR */
                        Return (BUF0) /* \_SB_.PCI0.PX40.PS2M._CRS.BUF0 */
                    }

                    Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
                    {
                        IRQ (Edge, ActiveHigh, Exclusive, )
                            {3,4,5,7,10,12}
                    })
                    Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
                    {
                        CreateWordField (Arg0, 0x01, IRQW)
                        Local0 = (0x03 & KBIN) /* \_SB_.PCI0.PX40.KBIN */
                        While (Local0)
                        {
                            Local1 = KBDA /* \_SB_.PCI0.PX40.KBDA */
                            Local0 = (0x03 & KBIN) /* \_SB_.PCI0.PX40.KBIN */
                        }

                        KBIN = 0xA8
                        LDNM = 0x05
                        FindSetRightBit (IRQW, Local0)
                        Local0--
                        INTR = Local0
                        ACTR = One
                    }
                }
            }

            Device (PX43)
            {
                Name (_ADR, 0x00070003)  // _ADR: Address
                OperationRegion (IPMU, PCI_Config, 0x40, 0x02)
                Field (IPMU, ByteAcc, NoLock, Preserve)
                {
                    PM00,   8, 
                    PM01,   8
                }

                OperationRegion (ISMB, PCI_Config, 0x90, 0x02)
                Field (ISMB, ByteAcc, NoLock, Preserve)
                {
                    SB00,   8, 
                    SB01,   8
                }
            }

            Device (BX00)
            {
                Name (_ADR, 0x00)  // _ADR: Address
            }

            Device (USB0)
            {
                Name (_ADR, 0x00070002)  // _ADR: Address
                Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
                {
                    0x08, 
                    0x01
                })
            }

            Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
            {
                0x09, 
                0x01
            })
        }
    }

    Scope (\_GPE)
    {
        Method (_L08, 0, Serialized)  // _Lxx: Level-Triggered GPE, xx=0x00-0xFF
        {
            Notify (\_SB.PCI0.USB0, 0x02) // Device Wake
        }

        Method (_L0A, 0, Serialized)  // _Lxx: Level-Triggered GPE, xx=0x00-0xFF
        {
            Notify (\_SB.PCI0.PX40.UAR1, 0x02) // Device Wake
            Notify (\_SB.PCI0.PX40.UAR2, 0x02) // Device Wake
        }

        Method (_L09, 0, Serialized)  // _Lxx: Level-Triggered GPE, xx=0x00-0xFF
        {
            Notify (\_SB.PCI0, 0x02) // Device Wake
        }
    }
}

