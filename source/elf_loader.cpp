#include "elf_loader.h"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <format>
#include <istream>
#include <span>
#include <vector>

namespace
{
    enum
    {
        EI_MAG0 = 0,
        EI_MAG1,
        EI_MAG2,
        EI_MAG3,
        EI_CLASS,
        EI_DATA,
        EI_VERSION
    };

    enum
    {
        PF_X = 1,
        PF_W = 2,
        PF_R = 4
    };

    enum
    {
        ET_EXEC = 2
    };

    enum
    {
        EM_RISCV = 0xf3
    };

    enum
    {
        PT_LOAD = 1
    };
} // namespace

namespace elf
{

    std::string to_string(Error err)
    {
        using enum Error;
        switch (err)
        {
        case ER_OK:
            return "ok";
        case ER_BAD_ELF:
            return "badly formatted ELF file";
        case ER_INVALID_ARGUMENT:
            return "invalid argument";
        case ER_IO_FAILED:
            return "i/o failed";
        case ER_NOT_SUPPORTED:
            return "not supported";
        case ER_ALLOCATION_FAILED:
            return "allocation failed";
        }
        return "unknown";
    }

    Result<Ehdr> ReadElfHeader(std::istream& is, uint32_t fileSize)
    {
        // Read the ELF header.
        Ehdr ehdr{};

        if (!is.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr)))
        {
            return std::unexpected(Error::ER_IO_FAILED);
        }

        // Check the magic number.
        const auto& e_ident = ehdr.e_ident;
        if (e_ident[EI_MAG0] != '\x7f' || e_ident[EI_MAG1] != 'E' || e_ident[EI_MAG2] != 'L'
            || e_ident[EI_MAG3] != 'F')
        {
            return std::unexpected(Error::ER_BAD_ELF);
        }

        // Check that it is 32-bit.
        if (e_ident[EI_CLASS] != 1)
        {
            return std::unexpected(Error::ER_NOT_SUPPORTED);
        }

        // Check that it is two's complement, little-endian.
        if (e_ident[EI_DATA] != 1)
        {
            return std::unexpected(Error::ER_NOT_SUPPORTED);
        }

        // Check the ident version.
        if (e_ident[EI_VERSION] != 1)
        {
            return std::unexpected(Error::ER_NOT_SUPPORTED);
        }

        // Check that it is an executable.
        if (ehdr.e_type != ET_EXEC)
        {
            return std::unexpected(Error::ER_NOT_SUPPORTED);
        }

        // Check that it is for RISC-V.
        if (ehdr.e_machine != EM_RISCV)
        {
            return std::unexpected(Error::ER_NOT_SUPPORTED);
        }

        // Check the version.
        if (ehdr.e_version != 1)
        {
            return std::unexpected(Error::ER_NOT_SUPPORTED);
        }

        // Check that the size of a program header entry is what we expect it to be.
        if (ehdr.e_phentsize != sizeof(Phdr))
        {
            return std::unexpected(Error::ER_NOT_SUPPORTED);
        }

        // Check that the program header table is beyond the ELF header and within the file.
        // TODO: overflow, overlap.
        if (ehdr.e_phoff < sizeof(ehdr)
            || ehdr.e_phoff + ehdr.e_phentsize * ehdr.e_phnum > fileSize)
        {
            return std::unexpected(Error::ER_BAD_ELF);
        }

        return ehdr;
    }

    Result<Phdr> ReadProgramHeader(std::istream& is, uint32_t fileSize)
    {
        // Read a program header.
        Phdr phdr{};
        if (!is.read(reinterpret_cast<char*>(&phdr), sizeof(phdr)))
        {
            return std::unexpected(Error::ER_IO_FAILED);
        }

        // The file size may not be larger than the memory size.
        if (phdr.p_filesz > phdr.p_memsz)
        {
            return std::unexpected(Error::ER_BAD_ELF);
        }

        // TODO: other validation.

        return phdr;
    }

    Result<void> ReadSegment(std::istream& is, const Phdr& phdr, MemorySpan dst)
    {
        // Is the destination big enough?
        if (phdr.p_memsz > dst.size_bytes())
        {
            return std::unexpected(Error::ER_INVALID_ARGUMENT);
        }

        // Zero the destination up to p_memsz.
        std::ranges::fill_n(dst.begin(), phdr.p_memsz, std::byte{});

        // Copy data up to p_filesz (we know that p_filesz <= p_memsz).
        if (phdr.p_filesz != 0)
        {
            char* dstData = reinterpret_cast<char*>(dst.data());
            if (!is.seekg(phdr.p_offset) || !is.read(dstData, phdr.p_filesz))
            {
                return std::unexpected(Error::ER_IO_FAILED);
            }
        }

        return {};
    }

    namespace
    {
        Segments& UpdateSegmentAddresses(const Phdr& phdr, Segments& segments)
        {
            const auto paddr = phdr.p_paddr;
            const auto vaddr = phdr.p_vaddr;
            const auto memsz = phdr.p_memsz;
            const auto filesz = phdr.p_filesz;

            auto& lma = segments.lma;
            auto& vma = segments.vma;
            auto& startRom = segments.startRom;
            auto& endRom = segments.endRom;

            if (phdr.p_flags & PF_X)
            {
                // Executable segment.
                lma.startText = paddr;
                lma.endText = paddr + filesz;
                vma.startText = vaddr;
                vma.endText = vaddr + filesz;
                startRom = paddr;
                endRom = paddr + filesz;
            }
            else if (phdr.p_flags == PF_R)
            {
                // Read-only segment.
                lma.startRoData = paddr;
                lma.endRoData = paddr + filesz;
                vma.startRoData = vaddr;
                vma.endRoData = vaddr + filesz;
                endRom = paddr + filesz;
            }
            else if (phdr.p_flags == (PF_R | PF_W))
            {
                // Read/write segment.
                if (memsz == filesz)
                {
                    // Initialized data only.
                    lma.startData = paddr;
                    lma.endData = paddr + filesz;
                    vma.startData = vaddr;
                    vma.endData = vaddr + filesz;
                    if (paddr != vaddr)
                    {
                        // The initialized data is copied from read-only memory to read-write
                        // memory.
                        endRom = paddr + filesz;
                    }
                }
                else if (filesz == 0)
                {
                    // Uninitialized data only.
                    lma.startBss = paddr;
                    lma.endBss = paddr + memsz;
                    vma.startBss = vaddr;
                    vma.endBss = vaddr + memsz;
                }
                else
                {
                    // Both initialized data and uninitialized data.
                    lma.startBss = paddr + filesz;
                    lma.endBss = paddr + memsz;
                    vma.startBss = vaddr;
                    vma.endBss = vaddr + memsz;
                    lma.startData = paddr;
                    lma.endData = paddr + filesz;
                    vma.startData = vaddr;
                    vma.endData = vaddr + filesz;
                }
            }

            return segments;
        }
    } // namespace

    Result<Segments> LoadExecutable(std::istream& is, std::streampos fileSize,
                                    AllocatorFn allocateSegment)
    {
        auto ehdr = ReadElfHeader(is, fileSize);
        if (!ehdr)
        {
            return std::unexpected(ehdr.error());
        }

        // Go to the start of the program header table.
        if (!is.seekg(ehdr->e_phoff))
        {
            return std::unexpected(Error::ER_IO_FAILED);
        }

        // Read the program headers.
        std::vector<Phdr> phdrs;
        phdrs.reserve(ehdr->e_phnum);
        for (uint16_t i = 0; i < ehdr->e_phnum; i++)
        {
            auto phdr = ReadProgramHeader(is, fileSize);
            if (!phdr)
            {
                return std::unexpected(phdr.error());
            }
            if (phdr->p_type == PT_LOAD)
            {
                phdrs.push_back(*phdr);
            }
        }

        Segments segments{};

        // Set the entry point.
        segments.entry = ehdr->e_entry;

        // Load each loadable segment.
        for (const auto& phdr : phdrs)
        {
            UpdateSegmentAddresses(phdr, segments);

            // Get some memory for the segment.
            auto segment = allocateSegment(phdr);
            if (!segment)
            {
                return std::unexpected(segment.error());
            }

            // Read it.
            auto result = ReadSegment(is, phdr, *segment);
            if (!result)
            {
                return std::unexpected(result.error());
            }
        }
        return segments;
    }

} // namespace elf
