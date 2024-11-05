#include "elf_loader.h"

#include <format>
#include <iostream>
#include <string>

int main()
{
    const char path[] = "../../target/out/a.out";

    ElfLoader loader(path);
    if (!loader)
    {
        std::cout << std::format("Failed to load {}, {}\n", path, to_string(loader.error()));
        return 1;
    }

    // Display the program headers.
    std::cout << "Program Headers:\n";
    for (const auto& phdr : loader.ProgramHeaders())
    {
        if (phdr.p_type == PT_LOAD)
        {
            // Classify it based on flags and sizes.
            std::string type{};
            const auto& flags = phdr.p_flags;
            if (flags == (PF_R | PF_X))
            {
                type = "TEXT (r-x)";
            }
            else if (flags == PF_R && phdr.p_filesz != 0)
            {
                type = "RODATA (r--)";
            }
            else if (flags == (PF_R | PF_W) && phdr.p_filesz != 0)
            {
                type = "DATA (rw-)";
            }
            else if (flags == (PF_R | PF_W) && phdr.p_filesz == 0)
            {
                type = "BSS (rw-)";
            }
            else
            {
                type = "UNKNOWN";
            }

            std::cout << std::format("Found loadable {} segment\n", type);
            std::cout << std::format("\tp_offset = {:08x}\n", phdr.p_offset);
            std::cout << std::format("\t p_paddr = {:08x}\n", phdr.p_paddr);
            std::cout << std::format("\t p_vaddr = {:08x}\n", phdr.p_vaddr);
            std::cout << std::format("\tp_filesz = {:08x}\n", phdr.p_filesz);
            std::cout << std::format("\t p_memsz = {:08x}\n", phdr.p_memsz);
        }
    }

    // Display the section headers.
    std::cout << "\nSection Headers:\n";
    for (const auto& shdr : loader.SectionHeaders())
    {
        std::string name = loader.SectionName(shdr);
        std::cout << "section name: " << name << '\n';
        std::cout << std::format("\t  sh_flags = {:08x}\n", shdr.sh_flags);
        std::cout << std::format("\t   sh_type = {}\n", to_string(shdr.sh_type));
        std::cout << std::format("\t   sh_addr = {:08x}\n", shdr.sh_addr);
        std::cout << std::format("\t sh_offset = {:08x}\n", shdr.sh_offset);
        std::cout << std::format("\t   sh_size = {:08x}\n", shdr.sh_size);
        std::cout << std::format("\t   sh_info = {:08x}\n", shdr.sh_info);
    }

    return 0;
}
