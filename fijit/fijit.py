import fijit._fijs_ext as _ext
from typing import List, Dict

class Index:
    def __init__(self, text: str):
        if not isinstance(text, str):
            raise TypeError("Input text must be a string.")
        if not text:
            raise ValueError("Input text cannot be empty.")

        self._handle = _ext.create(text)
        if not self._handle:
            raise MemoryError("Failed to allocate FIJS index.")

    def search(self, patterns: List[str]) -> Dict[str, List[int]]:
        if not isinstance(patterns, list) or not all(isinstance(p, str) for p in patterns):
            raise TypeError("Patterns must be a list of strings.")

        if not self._handle:
            raise RuntimeError("Search called on an invalid or uninitialized Index.")

        return _ext.search(self._handle, patterns)

    def __repr__(self) -> str:
        return f"<{self.__class__.__name__} at {hex(id(self))}>"