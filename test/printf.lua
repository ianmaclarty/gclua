-- an implementation of printf

function printf(...)
 io.write(string.format(...))
end

printf("String: %s float: %.2f on int: %d\n", "blah", 3.14, 18)
