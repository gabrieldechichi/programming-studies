[points]
# x   y   z      r   g   b

# The base
-0.5 -0.3 -0.5    1.0 0.0 1.0 1.0
+0.5 -0.3 -0.5    0.0 1.0 0.0 1.0
+0.5 -0.3 +0.5    0.0 0.0 1.0 1.0
-0.5 -0.3 +0.5    1.0 0.0 1.0 1.0

# The tip of the pyramid (Y as the height)
+0.0 +0.5 +0.0    0.5 0.5 0.5 1.0

[indices]
# Base
0  1  2
0  2  3
# Sides
1 0 4
1 4 2
2 4 3
3 4 0
