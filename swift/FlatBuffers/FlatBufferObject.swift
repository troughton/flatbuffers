//
//  IFlatBufferObject.swift
//  FlatBuffers
//
//  Created by Thomas Roughton on 30/03/17.
//  Copyright © 2017 Thomas Roughton. All rights reserved.
//


/// <summary>
/// This is the base for both structs and tables.
/// </summary>
public protocol FlatbufferObject {
    init()
    
    mutating func __init(_ _i: Int, _ _bb: ByteBuffer)
    
    var byteBuffer : ByteBuffer { get }
}
