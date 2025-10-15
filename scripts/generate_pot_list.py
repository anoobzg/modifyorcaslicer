# Helps Generate localization/i18n/list.txt

import sys
import os

from pathlib import Path


def collect_file_names(path: Path):
    file_names = []
    base_dir = os.path.normpath(path)
    for root, dirs, files in os.walk(base_dir):
        rel_dir = os.path.relpath(root, base_dir)

        if rel_dir == '.':
            rel_dir = ''
        
        if not "src\\slic3r\\" in rel_dir:
            continue

        print("process rel_dir {0}".format(rel_dir))

        for file in files:
            if not file.endswith('.cpp'):
                continue

            print("process file {0}".format(file))
            if rel_dir:
                rel_path = os.path.join(rel_dir, file)
            else:
                rel_path = file

            file_names.append(rel_path)
    return file_names

def main():
    path_to_list_file = Path(sys.argv[0]).parent.parent / "localization" / "i18n" / "list.txt"
    path_source_dir = Path(sys.argv[0]).parent.parent
    print("Generate {0} from {1}".format(path_to_list_file, path_source_dir))

    file_names = collect_file_names(path_source_dir)

    libslic3r_files = [
        'src/libslic3r/GCode.cpp',
        'src/libslic3r/ExtrusionEntity.cpp',
        'src/libslic3r/Flow.cpp',
        'src/libslic3r/Format/AMF.cpp',
        'src/libslic3r/Zip/miniz_extension.cpp',
        'src/libslic3r/Preset.cpp',
        'src/libslic3r/Print.cpp',
        'src/libslic3r/PrintBase.cpp',
        'src/libslic3r/PrintConfig.cpp',
        'src/libslic3r/Zip/Zipper.cpp',
        'src/libslic3r/PrintObject.cpp',
        'src/libslic3r/PrintObjectSlice.cpp',
        'src/libslic3r/PlaceholderParser.cpp',
        'src/libslic3r/Support/TreeSupport.cpp',
        'src/libslic3r/Model.cpp',
        'src/libslic3r/Format/OBJ.cpp'
    ]
    file_names.extend(libslic3r_files)
    # print(file_names)
    with open(path_to_list_file, 'w') as list_file:
        for file_name in file_names:
            list_file.write(file_name + '\n')

    print("Generate SUCCESS")

if __name__ == "__main__":
    main()
