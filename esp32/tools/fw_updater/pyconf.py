# Contains Definitions for Reading and Writing to Config Files, and In the future definitions for encrypting necessary data.

import json

from updater import print_debug

try: 
    from local_settings import DEBUG
except:
    DEBUG = False


class Settings(object):

    def __init__(self, filename):
        self.__settings = {}
        self.__filename = filename
        self.__load()

    def __load(self):
        print_debug('Loading settings from : {}'.format(self.__filename), DEBUG)
        try:
            f = open(self.__filename, 'r')
            data = f.read()
            f.close()
            self.__settings = json.loads(data.strip())
            print_debug('Config read [OK]: {}'.format(self.__settings), DEBUG)
        except Exception as e:
            print_debug('Config read [ERROR]: {}'.format(e), DEBUG)
    
    def get(self, name, default=None, ignore=False):
        print_debug('Requesting {} from {}'.format(name, self.__settings))
        return default if ignore else self.__settings.get(name, default)

    def set(self, name, value):
        if value is not None:
            self.__settings[name] = value
        else:
            try:
                self.__settings.remove(name)
            except:
                pass
        self.__save()

    def __save(self):
        print_debug('Saving settings: {}'.format(self.__settings), DEBUG)
        try:
            data = json.dumps(self.__settings)
            f = open(self.__filename, 'w')
            f.write(data)
            f.close()
        except Exception as e:
            print_debug("Error saving settings: {}".format(e), DEBUG)
