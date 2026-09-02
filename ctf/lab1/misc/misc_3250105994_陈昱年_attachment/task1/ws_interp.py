data = open('calculator.lua', 'rb').read()

# Extract whitespace tokens from line 80 onwards
idx = 0
nl_count = 0
for i, b in enumerate(data):
    if b == 0x0a:
        nl_count += 1
        if nl_count == 79:
            idx = i + 1
            break

ws_data = data[idx:]
tokens = []
for b in ws_data:
    if b == 0x20:
        tokens.append('S')
    elif b == 0x09:
        tokens.append('T')
    elif b == 0x0a:
        tokens.append('L')

seq = ''.join(tokens)

# Whitespace interpreter
pos = 0
stack = []
heap = {}
output = []


def read_number():
    global pos
    sign = 1 if seq[pos] == 'S' else -1
    pos += 1
    num = 0
    while pos < len(seq) and seq[pos] != 'L':
        num = num * 2 + (0 if seq[pos] == 'S' else 1)
        pos += 1
    pos += 1  # skip L
    return sign * num


def read_label():
    global pos
    label = ''
    while pos < len(seq) and seq[pos] != 'L':
        label += seq[pos]
        pos += 1
    pos += 1  # skip L
    return label


while pos < len(seq):
    c = seq[pos]
    if c == 'S':
        pos += 1
        if pos >= len(seq):
            break
        c2 = seq[pos]
        if c2 == 'S':
            # Push number
            pos += 1
            num = read_number()
            stack.append(num)
        elif c2 == 'L':
            pos += 1
            if pos >= len(seq):
                break
            c3 = seq[pos]
            if c3 == 'S':
                pos += 1
                stack.append(stack[-1])
            elif c3 == 'T':
                pos += 1
                stack[-1], stack[-2] = stack[-2], stack[-1]
            elif c3 == 'L':
                pos += 1
                stack.pop()
        elif c2 == 'T':
            pos += 1
            if pos >= len(seq):
                break
            c3 = seq[pos]
            if c3 == 'S':
                pos += 1
                n = read_number()
                stack.append(stack[-(n + 1)])
            elif c3 == 'L':
                pos += 1
                n = read_number()
                top = stack.pop()
                for _ in range(n):
                    stack.pop()
                stack.append(top)
    elif c == 'T':
        pos += 1
        if pos >= len(seq):
            break
        c2 = seq[pos]
        if c2 == 'S':
            # Arithmetic
            pos += 1
            if pos >= len(seq):
                break
            c3 = seq[pos]
            pos += 1
            if c3 == 'S':
                if pos >= len(seq):
                    break
                c4 = seq[pos]
                pos += 1
                if c4 == 'S':
                    b_val = stack.pop()
                    a_val = stack.pop()
                    stack.append(a_val + b_val)
                elif c4 == 'T':
                    b_val = stack.pop()
                    a_val = stack.pop()
                    stack.append(a_val - b_val)
                elif c4 == 'L':
                    b_val = stack.pop()
                    a_val = stack.pop()
                    stack.append(a_val * b_val)
            elif c3 == 'T':
                if pos >= len(seq):
                    break
                c4 = seq[pos]
                pos += 1
                if c4 == 'S':
                    b_val = stack.pop()
                    a_val = stack.pop()
                    stack.append(a_val // b_val)
                elif c4 == 'T':
                    b_val = stack.pop()
                    a_val = stack.pop()
                    stack.append(a_val % b_val)
        elif c2 == 'T':
            # Heap access
            pos += 1
            if pos >= len(seq):
                break
            c3 = seq[pos]
            pos += 1
            if c3 == 'S':
                val = stack.pop()
                addr = stack.pop()
                heap[addr] = val
            elif c3 == 'T':
                addr = stack.pop()
                stack.append(heap.get(addr, 0))
        elif c2 == 'L':
            # I/O
            pos += 1
            if pos >= len(seq):
                break
            c3 = seq[pos]
            pos += 1
            if c3 == 'S':
                if pos >= len(seq):
                    break
                c4 = seq[pos]
                pos += 1
                if c4 == 'S':
                    val = stack.pop()
                    output.append(chr(val))
                elif c4 == 'T':
                    val = stack.pop()
                    output.append(str(val))
            elif c3 == 'T':
                pos += 1
                # Read (skip)
    elif c == 'L':
        pos += 1
        if pos >= len(seq):
            break
        c2 = seq[pos]
        if c2 == 'S':
            pos += 1
            if pos >= len(seq):
                break
            c3 = seq[pos]
            pos += 1
            if c3 == 'S':
                label = read_label()
            elif c3 == 'T':
                label = read_label()
            elif c3 == 'L':
                label = read_label()
        elif c2 == 'T':
            pos += 1
            if pos >= len(seq):
                break
            c3 = seq[pos]
            pos += 1
            if c3 == 'S':
                label = read_label()
                val = stack.pop()
            elif c3 == 'T':
                label = read_label()
                val = stack.pop()
            elif c3 == 'L':
                pass
        elif c2 == 'L':
            pos += 1
            if pos >= len(seq):
                break
            c3 = seq[pos]
            pos += 1
            if c3 == 'L':
                break

result = ''.join(output)
print('Flag:', result)
