from Qt cimport *

cdef extern from "../DolphinQt2/Resources.h":
    cdef cppclass Resources:
        @staticmethod
        void Init()

        @staticmethod
        QPixmap GetMisc(int id)

    # Can't get this to work nested inside the class
    enum:
        BANNER_MISSING "Resources::BANNER_MISSING"
        LOGO_LARGE "Resources::LOGO_LARGE"
        LOGO_SMALL "Resources::LOGO_SMALL"
