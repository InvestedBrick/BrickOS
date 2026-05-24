rm limine.conf
touch limine.conf
cat iso/limine_base.conf > limine.conf

find iso/modules -maxdepth 1 -type f -name '*.elf' -print0 | while IFS= read -r -d '' file; do
    rel=${file#iso/modules/}         
    printf '    module_path: boot():/modules/%s \n' "$rel" >> limine.conf
    printf '    module_cmdline: %s \n' "$rel" >> limine.conf
done

find iso/modules/images -maxdepth 1 -type f -name '*.qoi' -print0 | while IFS= read -r -d '' file; do
    rel=${file#iso/modules/images/}         
    printf '    module_path: boot():/modules/images/%s \n' "$rel" >> limine.conf
    printf '    module_cmdline: %s \n' "$rel" >> limine.conf
done