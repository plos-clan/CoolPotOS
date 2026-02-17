#include "cpu_features.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "term/klog.h"

cpuid_ecx_features_t featuresEcx;
cpuid_edx_features_t featuresEdx;
cpuid_ebx_features_t featuresEbx;

static void cpuid(cpuid_input_eax_t eax, cpuid_input_ecx_t ecx, cpuid_output_t *out) {
    __asm__ volatile("cpuid"
                     : "=a"(out->eax), "=b"(out->ebx), "=c"(out->ecx), "=d"(out->edx)
                     : "a"(eax), "c"(ecx));
}

static void cpuid_raw(uint32_t code, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(code) : "memory");
}

void arch_cpuid_feature_info(cpu_features_t *cpu_features) {
    cpuid_output_t out;
    cpuid(CPUID_EAX_FEATURE_INFO, CPUID_ECX_NONE, &out);
    featuresEcx = (cpuid_ecx_features_t)out.ecx;
    featuresEdx = (cpuid_edx_features_t)out.edx;
    cpuid(CPUID_EAX_EXTENDED_FEATURE_INFO, CPUID_ECX_NONE, &out);
    featuresEbx = (cpuid_ebx_features_t)out.ebx;

    bool status = true;
    if (featuresEdx & CPUID_EDX_SSE) {
        status &= string_builder_append(cpu_features->features, "sse ");
    }
    if (featuresEdx & CPUID_EDX_SSE2) {
        status &= string_builder_append(cpu_features->features, "sse2 ");
    }
    if (featuresEcx & CPUID_ECX_SSE3) {
        status &= string_builder_append(cpu_features->features, "sse3 ");
    }
    if (featuresEcx & CPUID_ECX_SSSE3) {
        status &= string_builder_append(cpu_features->features, "ssse3 ");
    }
    if (featuresEcx & CPUID_ECX_SSE4_1) {
        status &= string_builder_append(cpu_features->features, "sse4_1 ");
    }
    if (featuresEcx & CPUID_ECX_SSE4_2) {
        status &= string_builder_append(cpu_features->features, "sse4_2 ");
    }
    if (featuresEcx & CPUID_ECX_AVX) {
        status &= string_builder_append(cpu_features->features, "avx ");
    }
    if (featuresEbx & CPUID_EBX_AVX2) {
        status &= string_builder_append(cpu_features->features, "avx2 ");
    }
    if (featuresEbx & CPUID_EBX_AVX512F) {
        status &= string_builder_append(cpu_features->features, "avx512f ");
    }
    if (featuresEbx & CPUID_EBX_FSGSBASE)
        status &= string_builder_append(cpu_features->features, "fsgsbase ");
    if (featuresEbx & CPUID_EBX_TSC_ADJUST)
        status &= string_builder_append(cpu_features->features, "tsc_adjust ");
    if (featuresEbx & CPUID_EBX_SGX)
        status &= string_builder_append(cpu_features->features, "sgx ");
    if (featuresEbx & CPUID_EBX_BMI1)
        status &= string_builder_append(cpu_features->features, "bmi1 ");
    if (featuresEbx & CPUID_EBX_HLE)
        status &= string_builder_append(cpu_features->features, "hle ");
    if (featuresEbx & CPUID_EBX_AVX2)
        status &= string_builder_append(cpu_features->features, "avx2 ");
    if (featuresEbx & CPUID_EBX_FDP_EXCPTN_ONLY)
        status &= string_builder_append(cpu_features->features, "fdp_excptn_only ");
    if (featuresEbx & CPUID_EBX_SMEP)
        status &= string_builder_append(cpu_features->features, "smep ");
    if (featuresEbx & CPUID_EBX_BMI2)
        status &= string_builder_append(cpu_features->features, "bmi2 ");
    if (featuresEbx & CPUID_EBX_ERMS)
        status &= string_builder_append(cpu_features->features, "erms ");
    if (featuresEbx & CPUID_EBX_INVPCID)
        status &= string_builder_append(cpu_features->features, "invpcid ");
    if (featuresEbx & CPUID_EBX_RTM)
        status &= string_builder_append(cpu_features->features, "rtm ");
    if (featuresEbx & CPUID_EBX_RDT_M)
        status &= string_builder_append(cpu_features->features, "rdt_m ");
    if (featuresEbx & CPUID_EBX_FPU_CS_DS_DEPR)
        status &= string_builder_append(cpu_features->features, "fpu_cs_ds_depr ");
    if (featuresEbx & CPUID_EBX_MPX)
        status &= string_builder_append(cpu_features->features, "mpx ");
    if (featuresEbx & CPUID_EBX_RDT_A)
        status &= string_builder_append(cpu_features->features, "rdt_a ");
    if (featuresEbx & CPUID_EBX_AVX512F)
        status &= string_builder_append(cpu_features->features, "avx512f ");
    if (featuresEbx & CPUID_EBX_AVX512DQ)
        status &= string_builder_append(cpu_features->features, "avx512dq ");
    if (featuresEbx & CPUID_EBX_RDSEED)
        status &= string_builder_append(cpu_features->features, "rdseed ");
    if (featuresEbx & CPUID_EBX_ADX)
        status &= string_builder_append(cpu_features->features, "adx ");
    if (featuresEbx & CPUID_EBX_SMAP)
        status &= string_builder_append(cpu_features->features, "smap ");
    if (featuresEbx & CPUID_EBX_AVX512_IFMA)
        status &= string_builder_append(cpu_features->features, "avx512ifma ");
    if (featuresEbx & CPUID_EBX_CLFLUSHOPT)
        status &= string_builder_append(cpu_features->features, "clflushopt ");
    if (featuresEbx & CPUID_EBX_CLWB)
        status &= string_builder_append(cpu_features->features, "clwb ");
    if (featuresEbx & CPUID_EBX_INTEL_PT)
        status &= string_builder_append(cpu_features->features, "intel_pt ");
    if (featuresEbx & CPUID_EBX_AVX512PF)
        status &= string_builder_append(cpu_features->features, "avx512pf ");
    if (featuresEbx & CPUID_EBX_AVX512ER)
        status &= string_builder_append(cpu_features->features, "avx512er ");
    if (featuresEbx & CPUID_EBX_AVX512CD)
        status &= string_builder_append(cpu_features->features, "avx512cd ");
    if (featuresEbx & CPUID_EBX_SHA)
        status &= string_builder_append(cpu_features->features, "sha ");
    if (featuresEbx & CPUID_EBX_AVX512BW)
        status &= string_builder_append(cpu_features->features, "avx512bw ");
    if (featuresEbx & CPUID_EBX_AVX512VL)
        status &= string_builder_append(cpu_features->features, "avx512vl ");

    if (featuresEdx & CPUID_EDX_FPU)
        status &= string_builder_append(cpu_features->features, "fpu ");
    if (featuresEdx & CPUID_EDX_VME)
        status &= string_builder_append(cpu_features->features, "vme ");
    if (featuresEdx & CPUID_EDX_DE)
        status &= string_builder_append(cpu_features->features, "de ");
    if (featuresEdx & CPUID_EDX_PSE)
        status &= string_builder_append(cpu_features->features, "pse ");
    if (featuresEdx & CPUID_EDX_TSC)
        status &= string_builder_append(cpu_features->features, "tsc ");
    if (featuresEdx & CPUID_EDX_MSR)
        status &= string_builder_append(cpu_features->features, "msr ");
    if (featuresEdx & CPUID_EDX_PAE)
        status &= string_builder_append(cpu_features->features, "pae ");
    if (featuresEdx & CPUID_EDX_MCE)
        status &= string_builder_append(cpu_features->features, "mce ");
    if (featuresEdx & CPUID_EDX_CX8)
        status &= string_builder_append(cpu_features->features, "cx8 ");
    if (featuresEdx & CPUID_EDX_APIC)
        status &= string_builder_append(cpu_features->features, "apic ");
    if (featuresEdx & CPUID_EDX_SEP)
        status &= string_builder_append(cpu_features->features, "sep ");
    if (featuresEdx & CPUID_EDX_MTRR)
        status &= string_builder_append(cpu_features->features, "mtrr ");
    if (featuresEdx & CPUID_EDX_PGE)
        status &= string_builder_append(cpu_features->features, "pge ");
    if (featuresEdx & CPUID_EDX_MCA)
        status &= string_builder_append(cpu_features->features, "mca ");
    if (featuresEdx & CPUID_EDX_CMOV)
        status &= string_builder_append(cpu_features->features, "cmov ");
    if (featuresEdx & CPUID_EDX_PAT)
        status &= string_builder_append(cpu_features->features, "pat ");
    if (featuresEdx & CPUID_EDX_PSE36)
        status &= string_builder_append(cpu_features->features, "pse36 ");
    if (featuresEdx & CPUID_EDX_PSN)
        status &= string_builder_append(cpu_features->features, "psn ");
    if (featuresEdx & CPUID_EDX_CLFSH)
        status &= string_builder_append(cpu_features->features, "clfsh ");
    if (featuresEdx & CPUID_EDX_DS)
        status &= string_builder_append(cpu_features->features, "ds ");
    if (featuresEdx & CPUID_EDX_ACPI)
        status &= string_builder_append(cpu_features->features, "acpi ");
    if (featuresEdx & CPUID_EDX_MMX)
        status &= string_builder_append(cpu_features->features, "mmx ");
    if (featuresEdx & CPUID_EDX_FXSR)
        status &= string_builder_append(cpu_features->features, "fxsr ");
    if (featuresEdx & CPUID_EDX_SSE)
        status &= string_builder_append(cpu_features->features, "sse ");
    if (featuresEdx & CPUID_EDX_SSE2)
        status &= string_builder_append(cpu_features->features, "sse2 ");
    if (featuresEdx & CPUID_EDX_SS)
        status &= string_builder_append(cpu_features->features, "ss ");
    if (featuresEdx & CPUID_EDX_HTT)
        status &= string_builder_append(cpu_features->features, "htt ");
    if (featuresEdx & CPUID_EDX_TM)
        status &= string_builder_append(cpu_features->features, "tm ");
    if (featuresEdx & CPUID_EDX_PBE)
        status &= string_builder_append(cpu_features->features, "pbe ");

    if (featuresEcx & CPUID_ECX_SSE3)
        status &= string_builder_append(cpu_features->features, "sse3 ");
    if (featuresEcx & CPUID_ECX_PCLMULQDQ)
        status &= string_builder_append(cpu_features->features, "pclmulqdq ");
    if (featuresEcx & CPUID_ECX_DTES64)
        status &= string_builder_append(cpu_features->features, "dtes64 ");
    if (featuresEcx & CPUID_ECX_MONITOR)
        status &= string_builder_append(cpu_features->features, "monitor ");
    if (featuresEcx & CPUID_ECX_DS_CPL)
        status &= string_builder_append(cpu_features->features, "ds_cpl ");
    if (featuresEcx & CPUID_ECX_VMX)
        status &= string_builder_append(cpu_features->features, "vmx ");
    if (featuresEcx & CPUID_ECX_SMX)
        status &= string_builder_append(cpu_features->features, "smx ");
    if (featuresEcx & CPUID_ECX_EIST)
        status &= string_builder_append(cpu_features->features, "eist ");
    if (featuresEcx & CPUID_ECX_TM2)
        status &= string_builder_append(cpu_features->features, "tm2 ");
    if (featuresEcx & CPUID_ECX_SSSE3)
        status &= string_builder_append(cpu_features->features, "ssse3 ");
    if (featuresEcx & CPUID_ECX_CNXT_ID)
        status &= string_builder_append(cpu_features->features, "cnxt_id ");
    if (featuresEcx & CPUID_ECX_SDBG)
        status &= string_builder_append(cpu_features->features, "sdbg ");
    if (featuresEcx & CPUID_ECX_FMA)
        status &= string_builder_append(cpu_features->features, "fma ");
    if (featuresEcx & CPUID_ECX_CMPXCHG16B)
        status &= string_builder_append(cpu_features->features, "cx16 ");
    if (featuresEcx & CPUID_ECX_XTPR_UPDATE_CONTROL)
        status &= string_builder_append(cpu_features->features, "xtpr ");
    if (featuresEcx & CPUID_ECX_PDCM)
        status &= string_builder_append(cpu_features->features, "pdcm ");
    if (featuresEcx & CPUID_ECX_PCID)
        status &= string_builder_append(cpu_features->features, "pcid ");
    if (featuresEcx & CPUID_ECX_DCA)
        status &= string_builder_append(cpu_features->features, "dca ");
    if (featuresEcx & CPUID_ECX_SSE4_1)
        status &= string_builder_append(cpu_features->features, "sse4_1 ");
    if (featuresEcx & CPUID_ECX_SSE4_2)
        status &= string_builder_append(cpu_features->features, "sse4_2 ");
    if (featuresEcx & CPUID_ECX_X2APIC)
        status &= string_builder_append(cpu_features->features, "x2apic ");
    if (featuresEcx & CPUID_ECX_MOVBE)
        status &= string_builder_append(cpu_features->features, "movbe ");
    if (featuresEcx & CPUID_ECX_POPCNT)
        status &= string_builder_append(cpu_features->features, "popcnt ");
    if (featuresEcx & CPUID_ECX_TSC_DEADLINE)
        status &= string_builder_append(cpu_features->features, "tsc_deadline ");
    if (featuresEcx & CPUID_ECX_AESNI)
        status &= string_builder_append(cpu_features->features, "aes ");
    if (featuresEcx & CPUID_ECX_XSAVE)
        status &= string_builder_append(cpu_features->features, "xsave ");
    if (featuresEcx & CPUID_ECX_OSXSAVE)
        status &= string_builder_append(cpu_features->features, "osxsave ");
    if (featuresEcx & CPUID_ECX_AVX)
        status &= string_builder_append(cpu_features->features, "avx ");
    if (featuresEcx & CPUID_ECX_F16C)
        status &= string_builder_append(cpu_features->features, "f16c ");
    if (featuresEcx & CPUID_ECX_RDRAND)
        status &= string_builder_append(cpu_features->features, "rdrand ");

    int cpuid_level;
    static char x86_vendor_id[16] = { 0 };
    cpuid_raw(
        0x00000000,
        (uint32_t *)&cpuid_level,
        (uint32_t *)&x86_vendor_id[0],
        (uint32_t *)&x86_vendor_id[8],
        (uint32_t *)&x86_vendor_id[4]
    );
    cpu_features->vendor_id = strdup(x86_vendor_id);

    cpu_features->model_name = calloc(49, sizeof(char));
    uint32_t *v              = (uint32_t *)cpu_features->model_name;
    cpuid_raw(0x80000002, &v[0], &v[1], &v[2], &v[3]);
    cpuid_raw(0x80000003, &v[4], &v[5], &v[6], &v[7]);
    cpuid_raw(0x80000004, &v[8], &v[9], &v[10], &v[11]);
    cpu_features->model_name[48] = 0;

    uint32_t eax, ebx, ecx, edx;
    cpuid_raw(0x80000008, &eax, &ebx, &ecx, &edx);
    cpu_features->virt_bits = (eax >> 8) & 0xff;
    cpu_features->phys_bits = eax & 0xff;

    if (!status)
        kwarn("cannot build cpu feature.");
}
