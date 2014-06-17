class Matrix:
    def __init__(self, height, width):
        self.height = height;
        self.width = width;
        self.array = [0] * (height * width);

    def index(self, y, x):
    	assert(y >= 0);
	assert(y < self.height);
    	assert(x >= 0);
	assert(x < self.width);
	return y * self.width + x;

    def get(self, y, x):
        return self.array[self.index(y, x)];

    def put(self, y, x, v):
    	self.array[self.index(y, x)] = v;

    def multiply(self, other):
        assert(other.height == self.width);
	result = Matrix(self.height, other.width);

	sum = 0;

	y = 0;
	while (y < self.height):
	    x = 0;
	    while (x < other.width):
	    	k = 0;
		while (k < self.width):
		    sum = sum + self.get(y, k) * other.get(k, x);
		    k = k + 1;
		result.put(y, x, sum)
		x = x + 1;
	    y = y + 1;
	return result;

    def __repr__(self):
        return '%sx%s, %s' % (self.height, self.width, self.array)


z = 0;
b = None;
while (z < 200):
    a = Matrix(10, 10);
    b = Matrix(10, 10);

    y = 0;
    while (y < 10):
        x = 0;
        while (x < 10):
            a.put(y, x, (x + y) / 9);
            b.put(y, x, x * y / 33);
            x = x + 1;
        y = y + 1;

    y = 0;
    while (y < 5):
        b = a.multiply(b);
        y = y + 1
    z = z + 1

print(b);
