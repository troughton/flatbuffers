//
//  Struct.swift
//  FlatBuffers
//
//  Created by Thomas Roughton on 30/03/17.
//  Copyright © 2017 Thomas Roughton. All rights reserved.
//

/// <summary>
/// All structs in the generated code derive from this class, and add their own accessors.
/// </summary>
public struct Struct {
    public var bb_pos : Int = 0
    public var bb : ByteBuffer! = nil
    
    public init() {
        
    }
}
