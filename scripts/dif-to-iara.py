from __future__ import annotations
import re
import sys

IDENTIFIER = '([a-zA-Z_][a-zA-Z_0-9]*)'
INTEGER = '([0-9]+)'
COMMENT = r'(//[^\n]*\n)'
WHITESPACE = r'([ \t]+)'
SYMBOL = r'([:;={}])'

datatypes: dict[str, str] = {}


def translate_type(dif_type: str):
    if dif_type in datatypes:
        return f'!{dif_type}'
    match dif_type:
        case 'float':
            return 'f32'
        case 'int':
            return 'i32'
        case 'char':
            return 'i8'
        case _:
            return dif_type


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
    type: str
    value: str

    def __init__(self, parser: 'DIFParser' | None = None, name: str | None = None, type: str | None = None, value: str | None = None) -> None:
        if parser:
            self.name = parser.consume_identifier()
            parser.consume_token(':')
            self.type = parser.consume_identifier()
            parser.consume_token('=')
            self.value = parser.consume_integer()
            parser.consume_token(';')
        if name:
            self.name = name
        if type:
            self.type = type
        if value:
            self.value = value


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


class Actor:
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


class Datatype:
    name: str
    value: str

    def __init__(self, parser: 'DIFParser') -> None:
        self.name = parser.consume_identifier()
        parser.consume_token('=')
        self.value = parser.consume_integer()
        parser.consume_token(';')


class GraphBlock:
    name: str
    datatypes: list[Datatype]
    parameters: list[ParameterLine]
    delays: list[DelayLine]
    interfaces: list[InterfaceLine]
    actortypes: list[Actortype]
    actors: list[Actor]

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
                    self.actors.append(Actor(parser))
                case '}':
                    break
                case _:
                    raise Exception("Failed to match")
        parser.consume_token('}')

    def read_datatype_block(self, parser: 'DIFParser'):
        parser.consume_token('datatype')
        parser.consume_token('{')
        while parser.next_token() != '}':
            self.datatypes.append(Datatype(parser))
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

    def consume_integer(self):
        if re.match(INTEGER, self.next_token()):
            return self.consume_token()
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
                    name = interface.name
                    port = block.get_interface_port(name, self)
                    rv.append(Port(name=name, type=port.type, value=port.value))
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
                    name = interface.name
                    port = block.get_interface_port(name, self)
                    rv.append(Port(name=name, type=port.type, value=port.value))
                break
            for actortype in block.actortypes:
                if actortype.name == impl_name:
                    rv = actortype.production_ports
                    break
        self.prod_port_cache[impl_name] = rv
        return rv

    def get_formatted_production_types(self, impl_name: str) -> list[str]:
        return [f'tensor<{i.value}x{translate_type(i.type)}>' for i in self.get_production_ports(impl_name)]

    def get_formatted_consumption_types(self, impl_name: str) -> list[str]:
        return [f'tensor<{i.value}x{translate_type(i.type)}>' for i in self.get_consumption_ports(impl_name)]

    def to_iara(self) -> None:
        for block in self.graph_blocks:
            for datatype in block.datatypes:
                datatypes[datatype.name] = datatype.value
        for k, v in datatypes.items():
            # TODO: deal with alignment
            print(f"!{k} = tensor<{v}xi8>")
        for block in self.graph_blocks:
            if block.name == 'main':
                continue
            # populate known ports to differentiate from edge names
            print(f'iara.actor @{block.name} {{ // subgraph')
            input_interfaces: dict[str, dict[str, str]] = {}
            output_interfaces: dict[str, dict[str, str]] = {}
            in_ports: set[str] = set()
            out_ports: set[str] = set()
            for interface in block.interfaces:
                if interface.kind == 'consumption':
                    in_ports.add(interface.name)
                    input_interfaces[interface.name] = {}
                elif interface.kind == 'production':
                    out_ports.add(interface.name)
                    output_interfaces[interface.name] = {}

            for actor in block.actors:
                for input in self.get_consumption_ports(actor.type):
                    in_ports.add(input.name)
                for output in self.get_production_ports(actor.type):
                    out_ports.add(output.name)
                for lhs, rhs in actor.bindings:
                    if lhs in input_interfaces:
                        port_ = [
                            i for i in self.get_consumption_ports(actor.type) if i.name == rhs]
                        assert (len(port_) == 1)
                        port = port_[0]
                        input_interfaces[lhs] = {
                            'rate': port.value, 'type': port.type}
                    if rhs in output_interfaces:
                        port_ = [
                            i for i in self.get_production_ports(actor.type) if i.name == lhs]
                        assert (len(port_) == 1)
                        port = port_[0]
                        output_interfaces[rhs] = {
                            'rate': port.value, 'type': port.type}

            # IN ops

            for port in self.get_consumption_ports(block.name):
                assert len(v) > 0
                print(f'    %{port.name} = iara.in : tensor<{
                      port.value}x{translate_type(port.type)}>')

            # NODE ops

            for actor in block.actors:
                actor_in_ports: list[str] = []
                actor_out_ports: list[str] = []
                for input in self.get_consumption_ports(actor.type):
                    in_ports.add(input.name)
                    actor_in_ports.append(input.name)
                for output in self.get_production_ports(actor.type):
                    out_ports.add(output.name)
                    actor_out_ports.append(output.name)
                in_edges: list[str] = []
                out_edges: list[str] = []
                for lhs, rhs in actor.bindings:
                    if lhs in out_ports:
                        out_edges.append(rhs)
                    elif rhs in in_ports:
                        in_edges.append(lhs)
                    else:
                        raise Exception(f"Strange binding {lhs} -> {rhs}")
                print('    ', end='')
                if len(actor_out_ports) > 0:
                    print(', '.join(
                        [f'%{i}' for i in actor_out_ports]), '= ', end="")
                print(f'iara.node @{actor.type}', end='')

                if len(actor_in_ports) > 0:
                    print(f' in {', '.join([f'%{i}' for i in in_edges])} : {
                          ', '.join(self.get_formatted_consumption_types(actor.type))}', end='')
                if len(actor_out_ports) > 0:
                    print(
                        f' out : {', '.join(self.get_formatted_production_types(actor.type))}', end='')
                print("")

            # OUT ops

            for port in self.get_production_ports(block.name):
                type = f'tensor<{port.value}x{translate_type(port.type)}>'
                print(f'    iara.out ( %{port.name} : {type} ) : {type}',)

            print(f'    iara.dep\n}} // end subgraph {block.name}')


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
