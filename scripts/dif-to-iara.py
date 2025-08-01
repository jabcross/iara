from __future__ import annotations
import re
import sys

IDENTIFIER = '([a-zA-Z_][a-zA-Z_0-9]*)'
INTEGER = '([0-9]+)'
COMMENT = r'(//[^\n]*\n)'
WHITESPACE = r'([ \t]+)'
SYMBOL = r'([:;={}])'


class Datatype:
    def __init__(self, element_type, count) -> None:
        self.element_type: str = element_type
        self.count: int = count


datatypes: dict[str, Datatype] = {}


def collapse_tensors(type: str) -> str:
    if not type.startswith('tensor<'):
        return type
    if not "tensor" in type.removeprefix('tensor<'):
        return type
    new_type = re.sub(r'(.)tensor<([^>]*)>', r'\1\2', type)
    if new_type == type:
        return type
    return collapse_tensors(new_type)


def translate_type(dif_type: str | None) -> str | None:
    if dif_type is None:
        return None
    rv = ''
    if dif_type in datatypes:
        rv = f'tensor<{datatypes[dif_type].count}x{
            datatypes[dif_type].element_type}>'
    else:
        match dif_type:
            case 'float':
                rv = 'f32'
            case 'int':
                rv = 'i32'
            case 'char':
                rv = 'i8'
            case _:
                rv = dif_type
    return collapse_tensors(rv)


class ParameterLine:
    name: str
    type: str
    value: str

    def __init__(self, parser: 'DIFParser') -> None:
        self.name = parser.consume_identifier()
        parser.consume_token(':')
        self.type = parser.consume_identifier()
        parser.consume_token('=')
        self.value = parser.consume_integer()
        parser.consume_token(';')


class InterfaceLine:
    kind: str
    name: str

    def __init__(self, parser: 'DIFParser') -> None:
        self.kind = parser.consume_identifier()
        assert self.kind in ['consumption', 'production']
        self.name = parser.consume_identifier()
        parser.consume_token(';')


class DelayLine:
    name: str
    value: str

    def __init__(self, parser: 'DIFParser') -> None:
        self.name = parser.consume_identifier()
        parser.consume_token(':')
        self.value = parser.consume_integer()
        parser.consume_token(';')


class Port:
    name: str
    _type: str
    rate: int

    def __init__(self, parser: 'DIFParser' | None = None) -> None:
        if parser:
            self.name = parser.consume_identifier()
            parser.consume_token(':')
            self._type = parser.consume_identifier()
            parser.consume_token('=')
            self.rate = parser.consume_integer()
            parser.consume_token(';')


class Actortype:
    name: str
    parameters: list[str]
    consumption_ports: list[Port]
    production_ports: list[Port]

    def __init__(self, parser: 'DIFParser') -> None:
        self.parameters = []
        self.consumption_ports = []
        self.production_ports = []
        parser.consume_token('actortype')
        self.name = parser.consume_identifier()
        parser.consume_token('{')
        while parser.next_token() != '}':
            match parser.next_token():
                case 'param':
                    parser.consume_token('param')
                    self.parameters.append(parser.consume_identifier())
                    parser.consume_token(';')
                case 'consumption':
                    parser.consume_token('consumption')
                    self.consumption_ports.append(Port(parser))
                case 'production':
                    parser.consume_token('production')
                    self.production_ports.append(Port(parser))
        parser.consume_token('}')


class DifActor:
    name: str
    type: str
    parameters: list[str]
    bindings: list[tuple[str, str]]

    def __init__(self, parser: 'DIFParser') -> None:
        self.parameters = []
        self.bindings = []
        parser.consume_token('actor')
        self.name = parser.consume_identifier()
        parser.consume_token('{')
        while parser.next_token() != '}':
            match parser.next_token():
                case 'type':
                    parser.consume_token('type')
                    parser.consume_token(':')
                    self.type = parser.consume_identifier()
                    parser.consume_token(';')
                case 'param':
                    parser.consume_token('param')
                    self.parameters.append(parser.consume_identifier())
                    parser.consume_token(';')
                case 'interface':
                    parser.consume_token('interface')
                    lhs = parser.consume_identifier()
                    parser.consume_token('->')
                    rhs = parser.consume_identifier()
                    parser.consume_token(';')
                    self.bindings.append((lhs, rhs))
                case _:
                    raise Exception("Not matched")
        parser.consume_token('}')


class DIFDatatype:
    name: str
    value: str

    def __init__(self, parser: 'DIFParser') -> None:
        self.name = parser.consume_identifier()
        parser.consume_token('=')
        self.value = parser.consume_integer()
        parser.consume_token(';')


class GraphBlock:
    name: str
    datatypes: list[DIFDatatype]
    parameters: list[ParameterLine]
    delays: list[DelayLine]
    interfaces: list[InterfaceLine]
    actortypes: list[Actortype]
    actors: list[DifActor]

    def __init__(self, parser: 'DIFParser'):
        parser.consume_token('dif')
        self.name = parser.consume_identifier()
        parser.consume_token('{')
        self.datatypes = []
        self.parameters = []
        self.delays = []
        self.interfaces = []
        self.actortypes = []
        self.actors = []
        while True:
            match parser.next_token():
                case 'datatype':
                    self.read_datatype_block(parser)
                case 'parameter':
                    self.read_parameter_block(parser)
                case 'interface':
                    self.read_interface_block(parser)
                case 'delay':
                    self.read_delay_block(parser)
                case 'actortype':
                    self.actortypes.append(Actortype(parser))
                case 'actor':
                    self.actors.append(DifActor(parser))
                case '}':
                    break
                case _:
                    raise Exception("Failed to match")
        parser.consume_token('}')
        print("", flush=True)

    def read_datatype_block(self, parser: 'DIFParser'):
        parser.consume_token('datatype')
        parser.consume_token('{')
        while parser.next_token() != '}':
            self.datatypes.append(DIFDatatype(parser))
        parser.consume_token('}')

    def read_parameter_block(self, parser: 'DIFParser'):
        parser.consume_token('parameter')
        parser.consume_token('{')
        while parser.next_token() != '}':
            self.parameters.append(ParameterLine(parser))
        parser.consume_token('}')

    def read_interface_block(self, parser: 'DIFParser'):
        parser.consume_token('interface')
        parser.consume_token('{')
        while parser.next_token() != '}':
            self.interfaces.append(InterfaceLine(parser))
        parser.consume_token('}')

    def read_delay_block(self, parser: 'DIFParser'):
        parser.consume_token('delay')
        parser.consume_token('{')
        while parser.next_token() != '}':
            self.delays.append(DelayLine(parser))
        parser.consume_token('}')

    def get_interface_port(self, interface_name, parser: 'DIFParser'):
        for actor in self.actors:
            for lhs, rhs in actor.bindings:
                if lhs == interface_name:
                    ports = [p for p in parser.get_consumption_ports(
                        actor.type) if p.name == rhs]
                    if len(ports) > 0:
                        return ports[0]
                if rhs == interface_name:
                    ports = [p for p in parser.get_production_ports(
                        actor.type) if p.name == lhs]
                    if len(ports) > 0:
                        return ports[0]
        raise Exception(f'Tried to find interface {
                        interface_name} in graph {self.name}')


def format_tensor_type(rate: int, _type: str) -> str:
    if _type.startswith("tensor"):
        old_rate, old_type = _type.removeprefix(
            "tensor<").removesuffix(">").split('x')
        rate *= int(old_rate)
        return f'tensor<{rate}x{old_type}>'
    else:
        return f'tensor<{rate}x{_type}>'


class DIFParser:
    path: str
    tokens: list[str]
    graph_blocks: list[GraphBlock]

    def __init__(self, path) -> None:
        self.path = path
        self.tokenize()

    def tokenize(self):
        buff: str
        self.tokens = []
        with open(self.path, 'r') as file:
            buff = file.read()
        # print(f"buffer: {buff}")
        while len(buff) > 0:
            buff = buff.strip()
            match_ = re.search(
                f"^{IDENTIFIER}|{INTEGER}|{SYMBOL}|(->)|{COMMENT}|{WHITESPACE}", buff)
            if match_ is None:
                print("Failed match on buffer:")
                print(buff)
                raise Exception("Failed tokenization")
            tok: str = match_[0]
            buff = buff[len(tok):]
            if len(tok.strip()) > 0 and not tok.startswith('//'):
                self.tokens.append(tok)
        # print(self.tokens)

    def parse(self):
        self.graph_blocks = []
        while len(self.tokens) > 0:
            self.graph_blocks.append(GraphBlock(self))

    def consume_identifier(self):
        if re.match(IDENTIFIER, self.next_token()):
            return self.consume_token()
        raise Exception(f"expected identifier, got {self.next_token()}")

    def consume_integer(self) -> int:
        if re.match(INTEGER, self.next_token()):
            token = self.consume_token()
            return int(token)
        raise Exception(f"expected integer, got {self.next_token()}")

    def consume_token(self, tok: str | None = None):
        if tok is not None and self.tokens[0] != tok:
            raise Exception(f"Expected token {tok}, got {self.tokens[0]}")
        rv = self.tokens[0]
        self.tokens = self.tokens[1:]
        return rv

    def next_token(self):
        return self.tokens[0]

    cons_port_cache: dict[str, list[Port]] = {}

    def get_consumption_ports(self, impl_name: str) -> list[Port]:
        if impl_name in self.cons_port_cache:
            return self.cons_port_cache[impl_name]
        rv: list[Port] = []
        for block in self.graph_blocks:
            if impl_name == block.name:
                rv = []
                for interface in block.interfaces:
                    if interface.kind != 'consumption':
                        continue
                    name = interface.name
                    interface_port = block.get_interface_port(name, self)
                    port = Port()
                    port.name = name
                    port._type = interface_port._type
                    port.rate = interface_port.rate
                    assert (port.rate is not None)
                    rv.append(port)
                    # print(
                    # f'DEBUG {rv[-1].name} {rv[-1].type} {rv[-1].value}DEBUG')
                break
            for actortype in block.actortypes:
                if actortype.name == impl_name:
                    rv = actortype.consumption_ports
                    break
        self.cons_port_cache[impl_name] = rv
        return rv

    prod_port_cache: dict[str, list[Port]] = {}

    def get_production_ports(self, impl_name: str) -> list[Port]:
        if impl_name in self.prod_port_cache:
            return self.prod_port_cache[impl_name]
        rv: list[Port] = []
        for block in self.graph_blocks:
            if impl_name == block.name:
                rv = []
                for interface in block.interfaces:
                    if interface.kind != 'production':
                        continue
                    name = interface.name
                    interface_port = block.get_interface_port(name, self)
                    port = Port()
                    port.name = name
                    port._type = interface_port._type
                    port.rate = interface_port.rate
                    rv.append(port)
                break
            for actortype in block.actortypes:
                if actortype.name == impl_name:
                    rv = actortype.production_ports
                    break
        self.prod_port_cache[impl_name] = rv
        return rv

    def to_iara(self) -> None:

        print("module {")

        for block in self.graph_blocks:
            for datatype in block.datatypes:
                datatypes[datatype.name] = Datatype(f"!{datatype.name}", 1)
                # datatypes[datatype.name] = Datatype(
                #     'i8', int(datatype.value))
        # for k, v in datatypes.items():
        #     # TODO: deal with alignment
        #     print(f"!{k} = tensor<{v}xi8>")
        for block in self.graph_blocks:
            # if block.name == 'main':
            #     continue
            # populate known ports to differentiate from edge names
            print(f'iara.actor @{block.name} {{ // subgraph')
            input_interfaces: dict[str, dict[str, str]] = {}
            output_interfaces: dict[str, dict[str, str]] = {}
            in_ports: list[str] = []
            out_ports: list[str] = []
            for interface in block.interfaces:
                if interface.kind == 'consumption':
                    in_ports.append(interface.name)
                    input_interfaces[interface.name] = {}
                elif interface.kind == 'production':
                    out_ports.append(interface.name)
                    output_interfaces[interface.name] = {}

            for actor in block.actors:
                for input in self.get_consumption_ports(actor.type):
                    in_ports.append(input.name)
                for output in self.get_production_ports(actor.type):
                    out_ports.append(output.name)
                for lhs, rhs in actor.bindings:
                    if lhs in input_interfaces:
                        port_ = [
                            i for i in self.get_consumption_ports(actor.type) if i.name == rhs]
                        assert (len(port_) == 1)
                        port = port_[0]
                        input_interfaces[lhs] = {
                            'rate': port.rate, 'type': port._type}
                    if rhs in output_interfaces:
                        port_ = [
                            i for i in self.get_production_ports(actor.type) if i.name == lhs]
                        assert (len(port_) == 1)
                        port = port_[0]
                        output_interfaces[rhs] = {
                            'rate': port.rate, 'type': port._type}

            # IN ops

            for port in self.get_consumption_ports(block.name):
                print(
                    f'    %{port.name}_i_i = iara.in : {format_tensor_type(port.rate, translate_type(port._type))}')

            # NODE ops

            nodes_by_name: dict[str, NodeOp] = {}
            edges_by_name: dict[str, EdgeOp] = {}
            edge_delays: dict[str, str] = {}

            for delay_line in block.delays:
                edge_delays[delay_line.name] = delay_line.value

            class ActorInputPort:
                def __init__(self, actor: NodeOp) -> None:
                    self.actor: NodeOp = actor
                    self.port_name: str | None = None
                    self.value_name: str | None = None
                    self.type: str | None = None
                    self.rate: int | None = None
                    self.edge_op: EdgeOp | None = None
                    self.binding: tuple[str, str]

                def get_ssa_val_name(self) -> str:
                    if self.value_name is not None:
                        return self.value_name
                    if isinstance(self.edge_op, EdgeOp):
                        return self.edge_op.name + "_d"
                    else:
                        assert isinstance(self.binding, tuple)
                        return self.binding[0]

            class ActorOutputPort:
                def __init__(self, actor: NodeOp) -> None:
                    self.actor: NodeOp = actor
                    self.port_name: str | None = None
                    self.value_name: str | None = None
                    self.type: str | None = None
                    self.rate: int | None = None
                    self.edge_op: EdgeOp | None = None
                    self.binding: tuple | None

                def get_ssa_val_name(self) -> str:
                    if self.value_name is not None:
                        return self.value_name
                    if isinstance(self.edge_op, EdgeOp):
                        return self.edge_op.name + "_s"
                    else:
                        assert isinstance(self.binding, tuple)
                        return self.binding[1]

            class NodeOp:
                def __init__(self, name: str = "", type: str = "") -> None:
                    self.dif_actor: DifActor | None = None
                    self.name: str = name
                    self.type: str = type
                    self.in_ports: list[ActorInputPort] = []
                    self.out_ports: list[ActorOutputPort] = []

                def get_out_port_with_name(self, name: str) -> ActorOutputPort:
                    for i in self.out_ports:
                        if i.port_name == name:
                            return i
                    raise Exception(
                        f"output port {name} not found in actor {self.name}:{self.type}. dif actor: {self.dif_actor.parameters}")

                def get_in_port_with_name(self, name: str) -> ActorInputPort:
                    for i in self.in_ports:
                        if i.port_name == name:
                            return i
                    raise Exception(
                        f"input port {name} not found in actor {self.name}:{self.type}. dif actor: {self.dif_actor.parameters}")

                def format_result_ssa_values(self) -> str:
                    if len(self.out_ports) == 0:
                        return ""
                    return ', '.join([f'%{outport.get_ssa_val_name()}' for outport in self.out_ports]) + ' = '

                def format_input_ports(self) -> str:
                    if len(self.in_ports) == 0:
                        return ''
                    return f' in {', '.join(("%"+str(in_port.get_ssa_val_name()) for in_port in self.in_ports))}  : {', '.join((format_tensor_type(in_port.rate, translate_type(in_port.type)) for in_port in self.in_ports))} '

                def format_output_types(self) -> str:
                    assert (type(out_port.rate) is not None)
                    if len(self.out_ports) == 0:
                        return ''
                    return ' out: ' + ', '.join(format_tensor_type(out_port.rate, translate_type(out_port.type)) for out_port in self.out_ports) + ' '

                def format_op(self) -> str:
                    return f'{self.format_result_ssa_values()}iara.node @{self.type}{
                        self.format_input_ports()}{self.format_output_types()}'

            class EdgeOp:
                def __init__(self, name: str) -> None:
                    self.name: str = name
                    self.source_port: ActorOutputPort | None = None
                    self.drain_port: ActorInputPort | None = None
                    self.delay = None

                def format_attrs(self) -> str:
                    if self.name in edge_delays:
                        value_str: str = edge_delays[self.name]
                        try:
                            if type(value_str) is int:
                                value_int = value_str
                            elif type(value_str) is str:
                                # if int, delay is `value` default-constructed tokens
                                value_int: int = int(value_str, 10)
                            else:
                                assert (False)
                        except ValueError:
                            return ""
                        return f' {{ delay = {value_int} }}'
                    else:
                        return ""

                def format_op(self) -> str:
                    in_name = None
                    in_type = None
                    in_rate = 1
                    out_name = None
                    out_type = None
                    out_rate = 1
                    if isinstance(self.source_port, ActorOutputPort):
                        in_name = self.source_port.get_ssa_val_name()
                        in_type = translate_type(self.source_port.type)
                        in_rate = self.source_port.rate
                    elif self.name in input_interfaces:
                        in_name = self.name + "_i_i"
                        in_rate = input_interfaces[self.name]["rate"]
                        in_type = translate_type(
                            input_interfaces[self.name]["type"])
                    else:
                        assert False

                    if isinstance(self.drain_port, ActorInputPort):
                        out_name = self.drain_port.get_ssa_val_name()
                        out_type = translate_type(self.drain_port.type)
                        out_rate = self.drain_port.rate
                    elif self.name in output_interfaces:
                        out_name = self.name + '_o_i'
                        out_type = translate_type(
                            output_interfaces[self.name]["type"])
                        out_rate = output_interfaces[self.name]["rate"]
                    else:
                        assert False
                    assert isinstance(in_type, str)
                    return f'%{out_name} = iara.edge %{in_name} : {format_tensor_type(in_rate, in_type)} -> {format_tensor_type(out_rate, out_type)}{self.format_attrs()}'

            for actor in block.actors:
                actor_op: NodeOp = NodeOp()
                actor_op.dif_actor = actor
                actor_op.name = actor.name
                actor_op.type = actor.type
                nodes_by_name[actor.name] = actor_op

                cons_ports = self.get_consumption_ports(actor.type)
                prod_ports = self.get_production_ports(actor.type)

                assert len(cons_ports) + len(prod_ports) == len(actor.bindings)

                for input in self.get_consumption_ports(actor.type):
                    input_port: ActorInputPort = ActorInputPort(actor_op)
                    input_port.port_name = input.name
                    input_port.type = input._type
                    input_port.rate = input.rate
                    actor_op.in_ports.append(input_port)
                for output in self.get_production_ports(actor.type):
                    output_port: ActorOutputPort = ActorOutputPort(actor_op)
                    output_port.port_name = output.name
                    output_port.type = output._type
                    output_port.rate = output.rate
                    actor_op.out_ports.append(output_port)

                for lhs, rhs in actor.bindings:
                    if lhs in out_ports:
                        edge_name = rhs
                        if edge_name not in edges_by_name:
                            edges_by_name[edge_name] = EdgeOp(edge_name)
                        out_port: ActorOutputPort = actor_op.get_out_port_with_name(
                            lhs)
                        out_port.value_name = edge_name+'_s'
                        out_port.binding = lhs, rhs
                        edges_by_name[edge_name].source_port = out_port
                        out_port.edge_op = edges_by_name[edge_name]
                    elif rhs in in_ports:
                        edge_name = lhs
                        if edge_name not in edges_by_name:
                            edges_by_name[edge_name] = EdgeOp(edge_name)
                        in_port: ActorInputPort = actor_op.get_in_port_with_name(
                            rhs)
                        in_port.value_name = edge_name + '_d'
                        in_port.binding = lhs, rhs
                        edges_by_name[edge_name].drain_port = in_port
                        in_port.edge_op = edges_by_name[edge_name]
                    else:
                        raise Exception(f"Strange binding {lhs} -> {rhs}")

            # generate node ops

            for actor_op in nodes_by_name.values():
                print(f'   {actor_op.format_op()}')

            for edge_op in edges_by_name.values():
                print(f'   {edge_op.format_op()}')

            # OUT ops

            for port in self.get_production_ports(block.name):
                _type = format_tensor_type(
                    port.rate, translate_type(port._type))
                print(
                    f'    iara.out ( %{port.name}_o_i : {_type} ) : {_type}',)

            print(f'}} // end subgraph {block.name}')

        print("}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Specify DIF file.")
        exit(1)
    parser = DIFParser(sys.argv[1])
    try:
        parser.parse()
    except Exception as e:
        print(parser.tokens[:10])
        print(e)

    parser.to_iara()
