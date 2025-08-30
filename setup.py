from setuptools import setup, Extension

from pathlib import Path
this_directory = Path(__file__).parent
long_description = (this_directory / "README.md").read_text()

fijs_extension = Extension(
    'fijit._fijs_ext', 
    sources=[
        'fijit/_fijs_module.c',
        'src/fijs.c'
    ],
    include_dirs=['src/include'],
    language='c'
)

setup(
    name='fijit',
    version='0.1.0',
    author='vekoada',
    author_email='106115676+vekoada@users.noreply.github.com',
    description='A frequency-indexed jump search library for ludicrously fast multi-pattern searching in text.',
    long_description=long_description, 
    long_description_content_type='text/markdown',
    url='https://github.com/vekoada/fijit',
    packages=['fijit'],
    ext_modules=[fijs_extension],
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Intended Audience :: Developers",
        "Topic :: Text Processing :: Indexing",
    ],
    python_requires='>=3.8',
)