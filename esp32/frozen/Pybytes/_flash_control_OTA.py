import os

class FCOTA:
    def __init__(self):
        pass

    def update_file_content(self, path, newContent):
        print('Updating file [{}]'.format(path))

        if '.' in path:
            f = open(path, 'w')
            f.write(newContent)
            f.close()
            print('File updated')
            return True
        else:
            print('Cannot write into a folder')

        return False

    def delete_file(self, path):
        print('FCOTA deleting file [{}]'.format(path))
        try:
            if ('.' in path):
                os.remove(path)
            else:
                targetedFiles = []
                maxDepth = 0
                currentHierarchy = self.get_flash_hierarchy()
                for elem in currentHierarchy:
                    if path in elem:
                        targetedFiles.append(elem)
                        if elem.count('/') > maxDepth:
                            maxDepth = elem.count('/')
                if len(targetedFiles) > 0:
                    while maxDepth >= 0:
                        for elem in targetedFiles:
                            if elem.count('/') == maxDepth:
                                if '.' in elem:
                                    os.remove(elem)
                                else:
                                    os.rmdir(elem)
                        maxDepth -= 1
                else:
                    print('targetedFiles empty, no file to delete')
            return True
        except Exception as ex:
            print('FCOTA file deletion failed: {}'.format(ex))
            return False

    def convert_bytes(self, num):
        for x in ['bytes', 'KB', 'MB', 'GB', 'TB']:
            if num < 1024.0:
                return "%3.3g %s" % (num, x)
            num /= 1024.0

    def get_file_size(self, path):
        print('FCOTA getting file infos [{}]'.format(path))
        if '.' in path:
            fileInfo = os.stat(path)
            print (fileInfo)
            return self.convert_bytes(fileInfo[6])
        return 'Unknown'

    def get_file_content(self, path):
        print('FCOTA reading file [{}]'.format(path))

        if '.' in path:
            f = open(path, 'r')
            content = f.read()
            f.close()
        else:
            content = 'folder: {}'.format(path)

        # print('encoding content')
        # print(hexlify(content))
        # content = hexlify(content)

        return content

    def get_flash_hierarchy(self):
        hierarchy = os.listdir()
        folders = []
        for elem in hierarchy:
            if '.' not in elem:
                folders.append(elem)

        while len(folders) > 0:
            i = 0
            checkedFolders = []
            foldersToCheck = []

            while i < len(folders):
                subFolders = os.listdir(folders[i])

                if len(subFolders) > 0:
                    j = 0
                    while j < len(subFolders):
                        path = folders[i] + '/' + subFolders[j]
                        hierarchy.append(path)

                        if '.' not in path:
                            foldersToCheck.append(path)

                        j += 1

                checkedFolders.append(folders[i])
                i += 1

            i = 0
            while i < len(checkedFolders):
                folders.remove(checkedFolders[i])
                i += 1

            i = 0
            while i < len(foldersToCheck):
                folders.append(foldersToCheck[i])
                i += 1

        return hierarchy
