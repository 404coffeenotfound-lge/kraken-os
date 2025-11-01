idf_component_register(
    SRCS 
        "hello/hello_app.c"
        # goodbye_app.c built as dynamic app (loaded from partition at runtime)
        # "goodbye/goodbye_app.c"
    INCLUDE_DIRS 
        "hello"
        # "goodbye"
    REQUIRES
        system
)
