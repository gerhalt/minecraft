#!/usr/bin/python
'''

Building the C extension

'''

from distutils.core import setup, Extension

setup (name = 'Minecraft',
       version = '1.0',
       description = 'Minecraft extension module',
       ext_modules = [
            Extension("minecraft", sources = ["minecraft.c", "chunk.c", "nbt.c", "region.c", "world.c"])
       ])

