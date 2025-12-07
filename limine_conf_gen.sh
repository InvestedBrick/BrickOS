rm limine.conf
touch limine.conf
cat iso/limine_base.conf > limine.conf

find iso/modules -maxdepth 1 -type f -name '*.bin' -print0 | while IFS= read -r -d '' file; do
    rel=${file#iso/modules/}         
    printf '    module_path: boot():/modules/%s \n' "$rel" >> limine.conf
    printf '    module_cmdline: %s \n' "$rel" >> limine.conf
done