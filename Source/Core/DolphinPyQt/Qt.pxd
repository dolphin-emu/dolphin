# We need to wrap some DolphinQt2 classes, that have Qt-based types.
# We could do that with sip, but I'm used to Cython (and I'm pretty
# sure Cython is easier).  So we need just enough Qt types to be able to
# wrap DolphinQt2 with Cython.

cdef extern from "QPixmap":
    cdef cppclass QPixmap:
        QPixmap(const QPixmap& pixmap)
        
