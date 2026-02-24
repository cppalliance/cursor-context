task<std::pair<error_code, dynamic_buffer>>
async_read_array(auto handle)
{
    dynamic_buffer buf;

    auto [ec, n] = co_await async_read(handle, as_writable_bytes(span(&buf.size, 1)));
    if (ec)
        co_return {ec, {}};

    buf.data = make_unique<byte[]>(buf.size);

    auto [ec2, n2] = co_await async_read(handle, span(buf.data.get(), buf.size));
    if (ec2)
        co_return {ec2, std::move(buf)};    // both values returned to caller

    co_return {{}, std::move(buf)};
}




















io_task<std::string> // returns [error_code, string]
read_body( tcp_socket& sock ) {
    std::string s;
    char buf[1024];
    for(;;) {
        auto [ec, n] = co_await sock.read_some( buf );
        s.append( buf, n );
        if( ec )
            co_return { ec, std::move(s) };
    }
}




io_task<> my_task();

{
    recycling_resource mr;
    io_context ioc;
                              // allocator
    run_async( ioc.get_executor(), &mr )( my_task() );
                                    // frame allocates
}




auto work = ex::on(sch, my_coroutine(conn)) // frame allocates here...
          | ex::then(process);

auto op = ex::connect(work, rcvr);          // ...allocator available here
ex::start(op);





std::execution::task< std::size_t >
do_read( tcp_socket& s, buffer& buf )
{
    auto [ec, n] = co_await s.async_read(buf);
    if (ec)
        co_yield with_error(ec);        // unfortunately, terminates the coroutine
    co_return n;
}

// set_value always
do_read( sock, buf )
  | then([](std::error_code ec, std::size_t n) {
        // ec and n both available - no loss
    })
  | upon_error([](auto e) {
        // never called - I/O errors travel in the pair
    })
  | upon_stopped([] {
        // never called - cancellation is in the pair
    });




auto [ec, n] = co_await sock.read(buf);





