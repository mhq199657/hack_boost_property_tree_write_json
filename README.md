# hack_boost_property_tree_write_json
A hack of boost's property_tree so that it can write numbers as is.

Boost's approach is to store everything as string; via and is populated via its `standard_callbacks`. 
We hack it so that for real strings we write char `"` and for numbers and boolean, not.

And we hack write_json to not escape the seen `"`
