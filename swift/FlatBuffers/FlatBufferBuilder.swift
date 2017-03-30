//
//  FlatBufferBuilder.swift
//  FlatBuffers
//
//  Created by Thomas Roughton on 30/03/17.
//  Copyright © 2017 Thomas Roughton. All rights reserved.
//

import Foundation

/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 Responsible for building up and accessing a FlatBuffer formatted byte array (via ByteBuffer).
 */
public final class FlatBufferBuilder {
    private var _space : Int = 0
    private var _bb : ByteBuffer
    private var _minAlign : Int = 1
    
    // The vtable for the current table (if _vtableSize >= 0)
    private var _vtable = [Int](repeating: 0, count: 16)
    // The size of the vtable. -1 indicates no vtable
    private var _vtableSize : Int = -1
    // Starting offset of the current struct/table.
    private var _objectStart : Int = 0
    // List of offsets of all vtables.
    private var _vtables = [Int](repeating: 0, count: 16)
    // Number of entries in `vtables` in use.
    private var _numVtables : Int = 0
    // For the current vector being built.
    private var _vectorNumElems = 0
    
    /// <summary>
    /// Create a FlatBufferBuilder with a given initial size.
    /// </summary>
    /// <param name="initialSize">
    /// The initial size to use for the internal buffer.
    /// </param>
    public init(initialSize: Int) {
        assert(initialSize > 0, "initialSize must be greater than zero.")
        _space = initialSize
        _bb = ByteBuffer(capacity: initialSize)
    }
    
    /// <summary>
    /// Reset the FlatBufferBuilder by purging all data that it holds.
    /// </summary>
    public func clear() {
        _space = _bb.length
        _bb.reset()
        _minAlign = 1
        while _vtableSize > 0 {
            _vtableSize -= 1
            _vtable[_vtableSize] = 0
        }
        _vtableSize = -1
        _objectStart = 0
        _numVtables = 0
        _vectorNumElems = 0
    }
    
    /// <summary>
    /// Gets and sets a Boolean to disable the optimization when serializing
    /// default values to a Table.
    ///
    /// In order to save space, fields that are set to their default value
    /// don't get serialized into the buffer.
    /// </summary>
    public var forceDefaults : Bool = false
    
    /// @cond FLATBUFFERS_INTERNAL
    
    public var offset : Int {
        get {
            return _bb.length - _space;
        }
    }
    
    public func pad(size: Int) {
        _space -= size
        _bb.putUInt8(offset: _space, value: 0, count: size)
    }
    
    // Doubles the size of the ByteBuffer, and copies the old data towards
    // the end of the new buffer (since we build the buffer backwards).
    func growBuffer() {
        let oldBuf = _bb.data
        let oldBufSize = oldBuf.count
        assert(oldBufSize & 0xC0000000 == 0, "FlatBuffers: cannot grow buffer beyond 2 gigabytes.")
        
        let newBufSize = oldBufSize << 1
        let newBuf = ByteBuffer(capacity: newBufSize)
        
        newBuf.data.baseAddress?.advanced(by: newBufSize - oldBufSize).copyBytes(from: oldBuf.baseAddress!, count: oldBufSize)
        
        _bb = newBuf
    }
    
    // Prepare to write an element of `size` after `additional_bytes`
    // have been written, e.g. if you write a string, you need to align
    // such the int length field is aligned to SIZEOF_INT, and the string
    // data follows it directly.
    // If all you need to do is align, `additional_bytes` will be 0.
    public func prep(size: Int, additionalBytes: Int) {
        // Track the biggest thing we've ever aligned to.
        if size > _minAlign {
            _minAlign = size
        }
        // Find the amount of alignment needed such that `size` is properly
        // aligned after `additional_bytes`
        let alignSize =
            ((~(_bb.length - _space + additionalBytes)) + 1) &
                (size - 1)
        // Reallocate the buffer if needed.
        while _space < alignSize + size + additionalBytes {
            let oldBufSize = _bb.length
            self.growBuffer()
            _space += _bb.length - oldBufSize
            
        }
        if alignSize > 0 {
            self.pad(size: alignSize)
        }
    }
    
    public func putBool(_ x: Bool) {
        _space -= MemoryLayout<UInt8>.size
        _bb.putUInt8(offset: _space, value: x ? 1 : 0)
    }
    
    public func putInt8(_ x: Int8) {
        _space -= MemoryLayout<Int8>.size
        _bb.putInt8(offset: _space, value: x)
    }
    
    public func putUInt8(_ x: UInt8) {
        _space -= MemoryLayout<UInt8>.size
        _bb.putUInt8(offset: _space, value: x)
    }
    
    public func putInt16(_ x: Int16) {
        _space -= MemoryLayout<Int16>.size
        _bb.putInt16(offset: _space, value: x)
    }
    
    public func putUInt16(_ x: UInt16) {
        _space -= MemoryLayout<UInt16>.size
        _bb.putUInt16(offset: _space, value: x)
    }
    
    public func putInt32(_ x: Int32) {
        _space -= MemoryLayout<Int32>.size
        _bb.putInt32(offset: _space, value: x)
    }
    
    public func putUInt32(_ x: UInt32) {
        _space -= MemoryLayout<UInt16>.size
        _bb.putUInt32(offset: _space, value: x)
    }
    
    public func putInt64(_ x: Int64) {
        _space -= MemoryLayout<Int64>.size
        _bb.putInt64(offset: _space, value: x)
    }
    
    public func putUInt64(_ x: UInt64) {
        _space -= MemoryLayout<UInt64>.size
        _bb.putUInt64(offset: _space, value: x)
    }
    
    public func putFloat(_ x: Float) {
        _space -= MemoryLayout<Float>.size
        _bb.putFloat(offset: _space, value: x)
    }
    
    public func putDouble(_ x: Double) {
        _space -= MemoryLayout<Double>.size
        _bb.putDouble(offset: _space, value: x)
    }
    
    /// <summary>
    /// Add a `bool` to the buffer (aligns the data and grows if necessary).
    /// </summary>
    /// <param name="x">The `bool` to add to the buffer.</param>
    public func addBool(_ x: Bool) {
        self.prep(size: MemoryLayout<UInt8>.size, additionalBytes: 0)
        self.putBool(x)
    }
    
    /// <summary>
    /// Add a `Int8` to the buffer (aligns the data and grows if necessary).
    /// </summary>
    /// <param name="x">The `Int8` to add to the buffer.</param>
    public func addInt8(_ x: Int8) {
        self.prep(size: MemoryLayout<Int8>.size, additionalBytes: 0)
        self.putInt8(x)
    }
    
    /// <summary>
    /// Add a `byte` to the buffer (aligns the data and grows if necessary).
    /// </summary>
    /// <param name="x">The `byte` to add to the buffer.</param>
    public func addUInt8(_ x: UInt8) {
        self.prep(size: MemoryLayout<UInt8>.size, additionalBytes: 0)
        self.putUInt8(x)
    }
    
    /// <summary>
    /// Add a `Int16` to the buffer (aligns the data and grows if necessary).
    /// </summary>
    /// <param name="x">The `Int16` to add to the buffer.</param>
    public func addInt16(_ x: Int16) {
        self.prep(size: MemoryLayout<Int16>.size, additionalBytes: 0)
        self.putInt16(x)
    }
    
    /// <summary>
    /// Add an `uInt16` to the buffer (aligns the data and grows if necessary).
    /// </summary>
    /// <param name="x">The `uInt16` to add to the buffer.</param>
    public func addUInt16(_ x: UInt16) {
        self.prep(size: MemoryLayout<UInt16>.size, additionalBytes: 0)
        self.putUInt16(x)
    }
    
    /// <summary>
    /// Add an `int` to the buffer (aligns the data and grows if necessary).
    /// </summary>
    /// <param name="x">The `int` to add to the buffer.</param>
    public func addInt32(_ x: Int32) {
        self.prep(size: MemoryLayout<Int32>.size, additionalBytes: 0)
        self.putInt32(x)
    }
    
    /// <summary>
    /// Add an `uint` to the buffer (aligns the data and grows if necessary).
    /// </summary>
    /// <param name="x">The `uint` to add to the buffer.</param>
    public func addUInt32(_ x: UInt32) {
        self.prep(size: MemoryLayout<UInt32>.size, additionalBytes: 0)
        self.putUInt32(x)
    }
    
    /// <summary>
    /// Add a `Int64` to the buffer (aligns the data and grows if necessary).
    /// </summary>
    /// <param name="x">The `Int64` to add to the buffer.</param>
    public func addInt64(_ x: Int64) {
        self.prep(size: MemoryLayout<Int64>.size, additionalBytes: 0)
        self.putInt64(x)
    }
    
    /// <summary>
    /// Add an `uInt64` to the buffer (aligns the data and grows if necessary).
    /// </summary>
    /// <param name="x">The `uInt64` to add to the buffer.</param>
    public func addUInt64(_ x: UInt64) {
        self.prep(size: MemoryLayout<UInt64>.size, additionalBytes: 0)
        self.putUInt64(x)
    }
    
    /// <summary>
    /// Add a `float` to the buffer (aligns the data and grows if necessary).
    /// </summary>
    /// <param name="x">The `float` to add to the buffer.</param>
    public func addFloat(_ x: Float) {
        self.prep(size: MemoryLayout<Float>.size, additionalBytes: 0)
        self.putFloat(x)
    }
    
    /// <summary>
    /// Add a `double` to the buffer (aligns the data and grows if necessary).
    /// </summary>
    /// <param name="x">The `double` to add to the buffer.</param>
    public func addDouble(_ x: Double) {
        self.prep(size: MemoryLayout<Double>.size, additionalBytes: 0)
        self.putDouble(x)
    }
    
    /// <summary>
    /// Adds an offset, relative to where it will be written.
    /// </summary>
    /// <param name="off">The offset to add to the buffer.</param>
    public func addOffset(_ off: Int) {
        self.prep(size: MemoryLayout<Int32>.size, additionalBytes: 0);  // Ensure alignment is already done.
        assert(Int(off) <= self.offset)
        
        let off = Int32(self.offset - off + MemoryLayout<Int32>.size)
        self.putInt32(off)
    }
    
    /// @cond FLATBUFFERS_INTERNAL
    public func startVector(elemSize: Int, count: Int, alignment: Int) {
        notNested()
        _vectorNumElems = count
        self.prep(size: MemoryLayout<Int32>.size, additionalBytes: elemSize * count)
        self.prep(size: alignment, additionalBytes: elemSize * count); // Just in case alignment > int.
    }
    /// @endcond
    
    /// <summary>
    /// Writes data necessary to finish a vector construction.
    /// </summary>
    public func endVector() -> VectorOffset {
        self.putInt32(Int32(_vectorNumElems))
        return VectorOffset(self.offset)
    }
    
    /// <summary>
    /// Creates a vector of tables.
    /// </summary>
    /// <param name="offsets">Offsets of the tables.</param>
    public func createVectorOfTables<T>(_ offsets: [Offset<T>]) -> VectorOffset {
        notNested()
        startVector(elemSize: MemoryLayout<Int32>.size, count: offsets.count, alignment: MemoryLayout<Int32>.size)
        
        for offset in offsets.reversed() {
            self.addOffset(offset.value)
        }
        return endVector()
    }
    
    /// @cond FLATBUFFERS_INTENRAL
    public func nested(obj: Int) {
        // Structs are always stored inline, so need to be created right
        // where they are used. You'll get this assert if you created it
        // elsewhere.
        assert(obj == self.offset, "FlatBuffers: struct must be serialized inline.")
    }
    
    public func notNested() {
        // You should not be creating any other objects or strings/vectors
        // while an object is being constructed
        assert(_vtableSize < 0, "FlatBuffers: object serialization must not be nested.")
    }
    
    public func startObject(numFields: Int) {
        assert(numFields >= 0, "Flatbuffers: invalid numFields")
        
        notNested()
        
        if (_vtable.count < numFields) {
            _vtable = [Int](repeating: 0, count: numFields)
        }
        
        _vtableSize = numFields
        _objectStart = self.offset
    }
    
    
    // Set the current vtable at `voffset` to the current location in the
    // buffer.
    public func slot(_ voffset: Int) {
        assert(voffset < _vtableSize, "Flatbuffers: invalid voffset")
        _vtable[voffset] = self.offset
    }
    
    /// <summary>
    /// Adds a Boolean to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addBool(_ o: Int, _ x: Bool, _ d: Bool) { if (self.forceDefaults || x != d) { addBool(x); slot(o); } }
    
    /// <summary>
    /// Adds a Int8 to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addInt8(_ o: Int, _ x: Int8, _ d: Int8) { if (self.forceDefaults || x != d) { addInt8(x); slot(o); } }
    
    /// <summary>
    /// Adds a Byte to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addUInt8(_ o: Int, _ x: UInt8, _ d: UInt8) { if (self.forceDefaults || x != d) { addUInt8(x); slot(o); } }
    
    /// <summary>
    /// Adds a Int16 to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addInt16(_ o: Int, _ x: Int16, _ d: Int16) { if (self.forceDefaults || x != d) { addInt16(x); slot(o); } }
    
    /// <summary>
    /// Adds a UInt16 to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addUInt16(_ o: Int, _ x: UInt16, _ d: UInt16) { if (self.forceDefaults || x != d) { addUInt16(x); slot(o); } }
    
    /// <summary>
    /// Adds an Int32 to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addInt32(_ o: Int, _ x: Int32, _ d: Int32) { if (self.forceDefaults || x != d) { addInt32(x); slot(o); } }
    
    /// <summary>
    /// Adds a UInt32 to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addUInt32(_ o: Int, _ x: UInt32, _ d: UInt32) { if (self.forceDefaults || x != d) { addUInt32(x); slot(o); } }
    
    /// <summary>
    /// Adds an Int64 to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addInt64(_ o: Int, _ x: Int64, _ d: Int64) { if (self.forceDefaults || x != d) { addInt64(x); slot(o); } }
    
    /// <summary>
    /// Adds a UInt64 to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addUInt64(_ o: Int, _ x: UInt64, _ d: UInt64) { if (self.forceDefaults || x != d) { addUInt64(x); slot(o); } }
    
    /// <summary>
    /// Adds a Single to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addFloat(_ o: Int, _ x: Float, _ d: Float) { if (self.forceDefaults || x != d) { addFloat(x); slot(o); } }
    
    /// <summary>
    /// Adds a Double to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addDouble(_ o: Int, _ x: Double, _ d: Double) { if (self.forceDefaults || x != d) { addDouble(x); slot(o); } }
    
    /// <summary>
    /// Adds a buffer offset to the Table at index `o` in its vtable using the value `x` and default `d`
    /// </summary>
    /// <param name="o">The index into the vtable</param>
    /// <param name="x">The value to self.put into the buffer. If the value is equal to the default
    /// and <see cref="self.forceDefaults"/> is false, the value will be skipped.</param>
    /// <param name="d">The default value to compare the value against</param>
    public func addOffset(_ o: Int, _ x: Int, _ d: Int) { if (self.forceDefaults || x != d) { addOffset(x); slot(o); } }
    /// @endcond
    
    /// <summary>
    /// Encode the string `s` in the buffer using UTF-8.
    /// </summary>
    /// <param name="s">The string to encode.</param>
    /// <returns>
    /// The offset in the buffer where the encoded string starts.
    /// </returns>
    public func createString(_ s: String) -> StringOffset {
        notNested()
        addUInt8(0)
        
        let utf8StringLen = s.utf8CString.count
        startVector(elemSize: 1, count: utf8StringLen, alignment: 1)
        _space -= utf8StringLen
        
        s.withCString { bytes in
            _bb.data.baseAddress!.advanced(by: _space).copyBytes(from: bytes, count: utf8StringLen)
        }
        
        return StringOffset(endVector().value)
    }
    
    /// @cond FLATBUFFERS_INTERNAL
    // Structs are stored inline, so nothing additional is being added.
    // `d` is always 0.
    public func addStruct(_ voffset: Int, _ x: Int, _ d: Int) {
        if x != d {
            nested(obj: x)
            slot(voffset)
        }
    }
    
    public func endObject() -> Int {
        assert(_vtableSize >= 0, "Flatbuffers: calling endObject without a startObject")
        
        addInt32(0)
        var vtableloc = self.offset
        
        // Write out the current vtable.
        for i in (0..<_vtableSize).reversed() {
            // Offset relative to the start of the table.
            let off = Int16(_vtable[i] != 0
                ? vtableloc - _vtable[i]
                : 0)
            addInt16(off)
            
            // clear out written entry
            _vtable[i] = 0
        }
        
        let standardFields = 2; // The fields below:
        addInt16(Int16(vtableloc - _objectStart))
        addInt16(Int16((_vtableSize + standardFields) *
            MemoryLayout<Int16>.size))
        
        // Search for an existing vtable that matches the current one.
        var existingVtable = 0
        for i in 0..<_numVtables {
            let vt1 = _bb.length - _vtables[i]
            let vt2 = _space
            let len = _bb.getInt16(at: vt1)
            if (len == _bb.getInt16(at: vt2)) {
                var skipAssignment = false
                for j in MemoryLayout<Int16>.size..<Int(len) {
                    
                    if (_bb.getInt16(at: vt1 + j) != _bb.getInt16(at: vt2 + j)) {
                        skipAssignment = true
                        break
                    }
                }
                if !skipAssignment {
                    existingVtable = _vtables[i]
                    break
                }
            }
        }
        
        if existingVtable != 0 {
            // Found a match:
            // Remove the current vtable.
            _space = _bb.length - vtableloc
            // Point table to existing vtable.
            _bb.putInt32(offset: _space, value: Int32(existingVtable - vtableloc))
        } else {
            // No match:
            // Add the location of the current vtable to the list of
            // vtables.
            if (_numVtables == _vtables.count) {
                
                _vtables.append(contentsOf: repeatElement(0, count: _numVtables))
            }
            _vtables[_numVtables] = self.offset
            _numVtables += 1
            // Point table to current vtable.
            _bb.putInt32(offset: _bb.length - vtableloc, value: Int32(self.offset - vtableloc))
        }
        
        _vtableSize = -1
        return vtableloc
    }
    
    // This checks a required field has been set in a given table that has
    // just been constructed.
    public func required(_ table: Int, _ field: Int) {
        let table_start = _bb.length - table
        let vtable_start = table_start - Int(_bb.getInt32(at: table_start))
        let ok = _bb.getInt16(at: vtable_start + field) != 0
        // If this fails, the caller will show what field needs to be set.
        precondition(ok, "FlatBuffers: field \(field) must be set")
    }
    /// @endcond
    
    /// <summary>
    /// Finalize a buffer, pointing to the given `root_table`.
    /// </summary>
    /// <param name="rootTable">
    /// An offset to be added to the buffer.
    /// </param>
    public func finish(rootTable: Int) {
        self.prep(size: _minAlign, additionalBytes: MemoryLayout<Int32>.size)
        self.addOffset(rootTable)
        _bb.position = _space
    }
    
    /// <summary>
    /// Get the ByteBuffer representing the FlatBuffer.
    /// </summary>
    /// <remarks>
    /// This is typically only called after you call `Finish()`.
    /// The actual data starts at the ByteBuffer's current position,
    /// not necessarily at `0`.
    /// </remarks>
    /// <returns>
    /// Returns the ByteBuffer for this FlatBuffer.
    /// </returns>
    public var dataBuffer : ByteBuffer { get { return _bb; } }
    
    /// <summary>
    /// A utility function to copy and return the ByteBuffer data as a
    /// `byte[]`.
    /// </summary>
    /// <returns>
    /// A full copy of the FlatBuffer data.
    /// </returns>
    public func sizedByteArray() -> [UInt8] {
        let baseAddress = _bb.data.baseAddress!.advanced(by: _bb.position)
        let length = _bb.data.count - _bb.position
        
        let newArray = [UInt8](UnsafeBufferPointer<UInt8>(start: baseAddress.assumingMemoryBound(to: UInt8.self), count: length))
        
        return newArray
    }
    
    /// <summary>
    /// Finalize a buffer, pointing to the given `rootTable`.
    /// </summary>
    /// <param name="rootTable">
    /// An offset to be added to the buffer.
    /// </param>
    /// <param name="fileIdentifier">
    /// A FlatBuffer file identifier to be added to the buffer before
    /// `root_table`.
    /// </param>
    public func finish(rootTable: Int, fileIdentifier: String) {
        self.prep(size: _minAlign, additionalBytes: MemoryLayout<Int32>.size +
            FlatBufferConstants.FileIdentifierLength)
        
        assert(fileIdentifier.utf8CString.count == FlatBufferConstants.FileIdentifierLength, "FlatBuffers: file identifier must be length \(FlatBufferConstants.FileIdentifierLength)")
    
        for i in (0..<FlatBufferConstants.FileIdentifierLength).reversed() {
            self.addUInt8(UInt8(bitPattern: fileIdentifier.utf8CString[i]))
        }

        self.finish(rootTable: rootTable)
    }
    
    
}
