module {
  dataflow {
    iara.kernel @hello(out o1: 1xi32);
    iara.kernel @world(in i1: 1xi32);
    %o1 = iara.actor @hello();
    iara.actor @world(%o1);
  }
}
